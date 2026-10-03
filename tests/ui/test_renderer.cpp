/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Tests for the renderer bridge: creation, per-slot buffer growth, and recording against an offscreen target.
 * @details These need a rendering device, like the rendering module's own tests, and take the same
 * CATALYST_RENDERING_VALIDATION switch. What they check is the bridge's own contract: the buffers a
 * batch lands in grow and stay grown, other slots are untouched, bad arguments are refused rather
 * than recorded, and a whole painted tree records and submits cleanly. Pixels are not read back: the
 * transfer path is a separate concern, and validation layers are the check on what was recorded.
 */

#include <catalyst/rendering/rendering.hpp>
#include <catalyst/ui/renderer.hpp>
#include <catalyst/ui/ui.hpp>

#include "../rendering/validation_option.hpp"
#include "../test_common.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

using namespace catalyst;

namespace
{
    constexpr rendering::extent2d k_target{64, 64};
    constexpr rendering::format k_format = rendering::format::rgba8_unorm;

    rendering::device make_device()
    {
        rendering::device dev =
            rendering::create_device(tests::with_validation({.application_name = "catalyst ui renderer tests"}));
        CT_REQUIRE(rendering::is_valid(dev));
        return dev;
    }

    ui::renderer_desc small_desc()
    {
        return {.color_format = k_format,
                .frames_in_flight = 2,
                .initial_vertices = 8,
                .initial_indices = 12,
                .debug_name = "ui test"};
    }

    /** @brief A colour target to record into, so the draws happen inside a real render pass. */
    struct offscreen
    {
        rendering::texture target{};

        explicit offscreen(const rendering::device &dev)
        {
            rendering::texture_desc td;
            td.extent = {k_target.width, k_target.height, 1};
            td.pixel_format = k_format;
            td.usage = rendering::texture_usage::render_target;
            td.debug_name = "ui test target";
            target = rendering::create_texture(dev, td);
            CT_REQUIRE(rendering::is_valid(target));
        }

        ~offscreen() { rendering::destroy_texture(target); }
    };

    /** @brief Records one pass that draws `batch` through `r` into `slot`, submits it and waits. */
    bool draw_and_wait(const rendering::device &dev, const offscreen &off, ui::renderer &r,
                       const ui::render_batch &batch, std::uint32_t slot, rendering::extent2d viewport = k_target)
    {
        rendering::command_list cl = rendering::create_command_list(dev, {.debug_name = "ui test"});
        CT_REQUIRE(rendering::is_valid(cl));

        const rendering::color_attachment color{
            .target = off.target, .load = rendering::load_op::clear, .store = rendering::store_op::store};

        CT_REQUIRE(rendering::begin_recording(cl));
        rendering::begin_render_pass(cl, {.color_attachments = std::span{&color, 1}, .debug_name = "ui test"});
        const bool drew = r.render(cl, batch, viewport, slot);
        rendering::end_render_pass(cl);
        CT_REQUIRE(rendering::end_recording(cl));

        const auto done = rendering::submit(rendering::get_queue(dev), cl);
        CT_REQUIRE(done.has_value());
        rendering::wait_idle(dev);
        rendering::pump(dev);
        rendering::destroy_command_list(cl);
        return drew;
    }

    /** @brief A tree of `boxes` solid rectangles side by side, painted into a batch. Each box is at least four
     * vertices. */
    void paint_boxes(std::size_t boxes, ui::render_batch &out)
    {
        ui::tree t;
        const ui::node root = t.create();
        t.mutable_style(root).width = ui::px(static_cast<float>(k_target.width));
        t.mutable_style(root).height = ui::px(static_cast<float>(k_target.height));
        t.mutable_style(root).background = ui::colors::black;

        for (std::size_t i = 0; i < boxes; ++i)
        {
            const ui::node child = t.create_child(root);
            ui::style &st = t.mutable_style(child);
            st.flex_grow = 1.0f;
            st.background = (i % 2 == 0) ? ui::colors::red : ui::colors::blue;
            st.border_radius = ui::corners_length::all(ui::px(4.0f)); // Rounded, so a box is many vertices.
        }

        const ui::extent viewport{static_cast<float>(k_target.width), static_cast<float>(k_target.height)};
        ui::layout(t, root, ui::layout_params::for_viewport(viewport));
        out.clear();
        ui::batch_builder builder(out, ui::rect::from_pos_size(ui::point{0.0f, 0.0f}, viewport));
        ui::paint(t, root, builder, ui::paint_params::for_viewport(viewport));
        CT_REQUIRE(!out.empty());
    }

    // ---------------------------------------------------------------------------------------------

    void test_create_rejects_bad_arguments()
    {
        rendering::device dev = make_device();

        auto zero_frames = ui::renderer::create(dev, {.frames_in_flight = 0});
        CT_REQUIRE(!zero_frames.has_value());
        CT_REQUIRE(zero_frames.error().code == rendering::error_code::invalid_argument);

        auto zero_vertices = ui::renderer::create(dev, {.initial_vertices = 0});
        CT_REQUIRE(!zero_vertices.has_value());
        CT_REQUIRE(zero_vertices.error().code == rendering::error_code::invalid_argument);

        auto no_device = ui::renderer::create(rendering::device{}, {});
        CT_REQUIRE(!no_device.has_value());
        CT_REQUIRE(no_device.error().code == rendering::error_code::invalid_argument);

        rendering::destroy_device(dev);
    }

    void test_create_and_destroy()
    {
        rendering::device dev = make_device();

        auto made = ui::renderer::create(dev, small_desc());
        CT_REQUIRE(made.has_value());
        ui::renderer &r = *made;

        CT_REQUIRE(r.valid());
        CT_REQUIRE(r.owner() == dev);
        CT_REQUIRE(r.frames_in_flight() == 2);
        CT_REQUIRE(r.vertex_capacity(0) == 8);
        CT_REQUIRE(r.index_capacity(0) == 12);
        CT_REQUIRE(r.vertex_capacity(1) == 8);
        CT_REQUIRE(r.vertex_capacity(2) == 0); // No such slot.
        CT_REQUIRE(r.desc().color_format == k_format);

        r.destroy();
        CT_REQUIRE(!r.valid());
        CT_REQUIRE(r.frames_in_flight() == 0);
        CT_REQUIRE(!r.owner());
        r.destroy(); // Idempotent.

        rendering::destroy_device(dev);
    }

    void test_move_transfers_ownership()
    {
        rendering::device dev = make_device();

        auto made = ui::renderer::create(dev, small_desc());
        CT_REQUIRE(made.has_value());

        ui::renderer moved(std::move(*made));
        CT_REQUIRE(moved.valid());
        CT_REQUIRE(moved.frames_in_flight() == 2);
        CT_REQUIRE(!made->valid());
        CT_REQUIRE(made->frames_in_flight() == 0);

        ui::renderer assigned;
        CT_REQUIRE(!assigned.valid());
        assigned = std::move(moved);
        CT_REQUIRE(assigned.valid());
        CT_REQUIRE(!moved.valid());

        rendering::destroy_device(dev);
    }

    void test_render_refuses_bad_arguments()
    {
        rendering::device dev = make_device();
        offscreen off(dev);

        auto made = ui::renderer::create(dev, small_desc());
        CT_REQUIRE(made.has_value());
        ui::renderer &r = *made;

        ui::render_batch batch;
        paint_boxes(1, batch);

        CT_REQUIRE(!draw_and_wait(dev, off, r, batch, 2));         // Slot out of range.
        CT_REQUIRE(!draw_and_wait(dev, off, r, batch, 0, {0, 0})); // Empty viewport.
        CT_REQUIRE(r.vertex_capacity(0) == 8);                     // Nothing was uploaded either way.

        ui::renderer invalid;
        rendering::command_list cl = rendering::create_command_list(dev, {.debug_name = "unused"});
        CT_REQUIRE(!invalid.render(cl, batch, k_target, 0));
        rendering::destroy_command_list(cl);

        r.destroy();
        rendering::destroy_device(dev);
    }

    void test_empty_batch_is_a_no_op()
    {
        rendering::device dev = make_device();
        offscreen off(dev);

        auto made = ui::renderer::create(dev, small_desc());
        CT_REQUIRE(made.has_value());
        ui::renderer &r = *made;

        const ui::render_batch empty;
        CT_REQUIRE(draw_and_wait(dev, off, r, empty, 0));
        CT_REQUIRE(r.vertex_capacity(0) == 8);
        CT_REQUIRE(r.index_capacity(0) == 12);

        r.destroy();
        rendering::destroy_device(dev);
    }

    void test_buffers_grow_per_slot_and_stay_grown()
    {
        rendering::device dev = make_device();
        offscreen off(dev);

        auto made = ui::renderer::create(dev, small_desc());
        CT_REQUIRE(made.has_value());
        ui::renderer &r = *made;

        ui::render_batch batch;
        paint_boxes(6, batch);
        CT_REQUIRE(batch.vertices.size() > 8);
        CT_REQUIRE(batch.indices.size() > 12);

        // The slot drawn into grows to fit; the other slot is untouched.
        CT_REQUIRE(draw_and_wait(dev, off, r, batch, 1));
        const std::uint32_t grown_vertices = r.vertex_capacity(1);
        const std::uint32_t grown_indices = r.index_capacity(1);
        CT_REQUIRE(grown_vertices >= batch.vertices.size());
        CT_REQUIRE(grown_indices >= batch.indices.size());
        CT_REQUIRE(r.vertex_capacity(0) == 8);
        CT_REQUIRE(r.index_capacity(0) == 12);

        // Drawing the same batch again reuses what it has: no reallocation.
        CT_REQUIRE(draw_and_wait(dev, off, r, batch, 1));
        CT_REQUIRE(r.vertex_capacity(1) == grown_vertices);
        CT_REQUIRE(r.index_capacity(1) == grown_indices);

        // A smaller batch afterwards does not shrink it either.
        ui::render_batch smaller;
        paint_boxes(1, smaller);
        CT_REQUIRE(draw_and_wait(dev, off, r, smaller, 1));
        CT_REQUIRE(r.vertex_capacity(1) == grown_vertices);

        r.destroy();
        rendering::destroy_device(dev);
    }

    void test_clip_outside_viewport_is_skipped()
    {
        rendering::device dev = make_device();
        offscreen off(dev);

        auto made = ui::renderer::create(dev, small_desc());
        CT_REQUIRE(made.has_value());
        ui::renderer &r = *made;

        // Hand-built: one triangle, drawn twice, once with a clip entirely off the target and once
        // with the builder's unbounded clip. Both must record without complaint; the first draws
        // nothing, the second clamps its scissor to the target.
        ui::render_batch batch;
        batch.vertices = {
            ui::vertex{{0.0f, 0.0f}, {}, 0xFF0000FFu},
            ui::vertex{{64.0f, 0.0f}, {}, 0xFF00FF00u},
            ui::vertex{{0.0f, 64.0f}, {}, 0xFFFF0000u},
        };
        batch.indices = {0, 1, 2};
        batch.commands = {
            ui::draw_command{
                .first_index = 0, .index_count = 3, .clip = ui::rect::from_xywh(-50.0f, -50.0f, 10.0f, 10.0f)},
            ui::draw_command{.first_index = 0, .index_count = 3, .clip = ui::batch_builder::unbounded_clip()},
            ui::draw_command{.first_index = 0, .index_count = 0, .clip = ui::batch_builder::unbounded_clip()},
        };

        CT_REQUIRE(draw_and_wait(dev, off, r, batch, 0));

        r.destroy();
        rendering::destroy_device(dev);
    }

    /** @brief Like `draw_and_wait`, but runs `prepare` outside the pass first. True only when both calls succeed. */
    bool prepare_draw_and_wait(const rendering::device &dev, const offscreen &off, ui::renderer &r,
                               const ui::render_batch &batch, std::uint32_t slot,
                               rendering::extent2d viewport = k_target)
    {
        rendering::command_list cl = rendering::create_command_list(dev, {.debug_name = "ui test"});
        CT_REQUIRE(rendering::is_valid(cl));

        const rendering::color_attachment color{
            .target = off.target, .load = rendering::load_op::clear, .store = rendering::store_op::store};

        CT_REQUIRE(rendering::begin_recording(cl));
        const bool prepared = r.prepare(cl, batch, viewport, slot);
        rendering::begin_render_pass(cl, {.color_attachments = std::span{&color, 1}, .debug_name = "ui test"});
        const bool drew = r.render(cl, batch, viewport, slot);
        rendering::end_render_pass(cl);
        CT_REQUIRE(rendering::end_recording(cl));

        const auto done = rendering::submit(rendering::get_queue(dev), cl);
        CT_REQUIRE(done.has_value());
        rendering::wait_idle(dev);
        rendering::pump(dev);
        rendering::destroy_command_list(cl);
        return prepared && drew;
    }

    /** @brief A tree with a translucent group holding overlapping children, painted with group opacity: a layered
     * batch. */
    void paint_group(ui::render_batch &out)
    {
        ui::tree t;
        const ui::node root = t.create();
        t.mutable_style(root).width = ui::px(static_cast<float>(k_target.width));
        t.mutable_style(root).height = ui::px(static_cast<float>(k_target.height));
        t.mutable_style(root).background = ui::colors::white;

        const ui::node group = t.create_child(root);
        t.mutable_style(group).width = ui::px(40.0f);
        t.mutable_style(group).height = ui::px(40.0f);
        t.mutable_style(group).background = ui::colors::red;
        t.mutable_style(group).opacity = 0.5f;

        const ui::node inner = t.create_child(group);
        t.mutable_style(inner).width = ui::px(20.0f);
        t.mutable_style(inner).height = ui::px(20.0f);
        t.mutable_style(inner).background = ui::colors::blue;
        t.mutable_style(inner).opacity = 0.5f; // Nested layer.

        const ui::extent viewport{static_cast<float>(k_target.width), static_cast<float>(k_target.height)};
        ui::layout(t, root, ui::layout_params::for_viewport(viewport));
        out.clear();
        ui::batch_builder builder(out, ui::rect::from_pos_size(ui::point{0.0f, 0.0f}, viewport));
        ui::paint(t, root, builder, ui::paint_params::for_viewport(viewport));
        CT_REQUIRE(out.layers.size() == 2u);
    }

    void test_texture_registry()
    {
        rendering::device dev = make_device();
        offscreen off(dev);

        auto made = ui::renderer::create(dev, small_desc());
        CT_REQUIRE(made.has_value());
        ui::renderer &r = *made;

        // The solid key, an invalid texture and a texture that cannot be sampled are all refused.
        CT_REQUIRE(!r.register_texture(ui::no_texture, rendering::texture{}));
        CT_REQUIRE(!r.register_texture(7u, rendering::texture{}));
        CT_REQUIRE(!r.register_texture(7u, off.target));
        CT_REQUIRE(!r.texture_of(7u));

        // A 2x2 single-channel atlas, as a glyph rasteriser would fill.
        rendering::texture_desc td;
        td.extent = {2, 2, 1};
        td.pixel_format = rendering::format::r8_unorm;
        td.usage = rendering::texture_usage::sampled;
        td.debug_name = "ui test atlas";
        const std::array<std::byte, 4> coverage = {std::byte{0xFF}, std::byte{0x00}, std::byte{0x00}, std::byte{0xFF}};
        rendering::texture atlas = rendering::create_texture(dev, td, coverage);
        CT_REQUIRE(rendering::is_valid(atlas));

        CT_REQUIRE(r.register_texture(7u, atlas));
        CT_REQUIRE(r.texture_of(7u) == atlas);
        CT_REQUIRE(!r.texture_of(8u));

        // Three commands: the atlas, a key nobody registered (draws solid), and solid colour.
        ui::render_batch batch;
        ui::batch_builder b(batch);
        b.set_texture(7u);
        b.add_rect(ui::rect::from_xywh(0.0f, 0.0f, 32.0f, 32.0f), ui::rect::from_xywh(0.0f, 0.0f, 1.0f, 1.0f),
                   ui::colors::white);
        b.set_texture(9u);
        b.add_rect(ui::rect::from_xywh(32.0f, 0.0f, 32.0f, 32.0f), ui::rect::from_xywh(0.0f, 0.0f, 1.0f, 1.0f),
                   ui::colors::white);
        b.set_texture(ui::no_texture);
        b.add_rect(ui::rect::from_xywh(0.0f, 32.0f, 64.0f, 32.0f), ui::colors::red);
        CT_REQUIRE(batch.commands.size() == 3u);
        CT_REQUIRE(draw_and_wait(dev, off, r, batch, 0));

        // Registering again replaces; unregistering drops the key, and the batch still draws.
        CT_REQUIRE(r.register_texture(7u, atlas));
        r.unregister_texture(7u);
        CT_REQUIRE(!r.texture_of(7u));
        r.unregister_texture(7u); // Idempotent.
        CT_REQUIRE(draw_and_wait(dev, off, r, batch, 0));

        r.destroy();
        rendering::destroy_texture(atlas);
        rendering::destroy_device(dev);
    }

    void test_layers_need_prepare_and_keep_their_images()
    {
        rendering::device dev = make_device();
        offscreen off(dev);

        auto made = ui::renderer::create(dev, small_desc());
        CT_REQUIRE(made.has_value());
        ui::renderer &r = *made;

        ui::render_batch batch;
        paint_group(batch);
        const auto layers = static_cast<std::uint32_t>(batch.layers.size());

        // Without prepare there are no layer images to composite, so render refuses the batch.
        CT_REQUIRE(!draw_and_wait(dev, off, r, batch, 0));
        CT_REQUIRE(r.layer_count(0) == 0);

        // Prepare then render: one image per visible layer, on that slot only, and the composite
        // quads sit in the buffers after the batch's own geometry.
        CT_REQUIRE(prepare_draw_and_wait(dev, off, r, batch, 1));
        CT_REQUIRE(r.layer_count(1) == layers);
        CT_REQUIRE(r.layer_count(0) == 0);
        CT_REQUIRE(r.layer_count(2) == 0); // No such slot.
        CT_REQUIRE(r.vertex_capacity(1) >= batch.vertices.size() + 4u * layers);
        CT_REQUIRE(r.index_capacity(1) >= batch.indices.size() + 6u * layers);

        // The images are reused frame to frame.
        CT_REQUIRE(prepare_draw_and_wait(dev, off, r, batch, 1));
        CT_REQUIRE(r.layer_count(1) == layers);

        // A layer entirely outside the viewport is never drawn and gets no image.
        ui::render_batch outside;
        ui::batch_builder b(outside);
        b.begin_layer(ui::rect::from_xywh(-100.0f, -100.0f, 50.0f, 50.0f), 0.5f);
        b.add_rect(ui::rect::from_xywh(-90.0f, -90.0f, 10.0f, 10.0f), ui::colors::red);
        b.end_layer();
        CT_REQUIRE(outside.layers.size() == 1u && outside.commands.size() == 1u);
        CT_REQUIRE(prepare_draw_and_wait(dev, off, r, outside, 0));
        CT_REQUIRE(r.layer_count(0) == 0);

        // A malformed batch (a layer that is its own parent) is refused before anything is recorded.
        ui::render_batch bad = batch;
        bad.layers[0].parent = 0u;
        CT_REQUIRE(!prepare_draw_and_wait(dev, off, r, bad, 0));

        // Prepare is optional for a batch without layers, and harmless.
        ui::render_batch plain;
        paint_boxes(1, plain);
        CT_REQUIRE(prepare_draw_and_wait(dev, off, r, plain, 0));

        // Preparing one batch and rendering another on the same slot is refused: the images and
        // quads belong to the batch that was prepared.
        {
            rendering::command_list cl = rendering::create_command_list(dev, {.debug_name = "ui test"});
            const rendering::color_attachment color{
                .target = off.target, .load = rendering::load_op::clear, .store = rendering::store_op::store};
            CT_REQUIRE(rendering::begin_recording(cl));
            CT_REQUIRE(r.prepare(cl, batch, k_target, 0));
            rendering::begin_render_pass(cl, {.color_attachments = std::span{&color, 1}, .debug_name = "ui test"});
            CT_REQUIRE(!r.render(cl, plain, k_target, 0));
            CT_REQUIRE(!r.render(cl, batch, k_target, 0)); // The refusal consumed the preparation.
            rendering::end_render_pass(cl);
            CT_REQUIRE(rendering::end_recording(cl));
            rendering::destroy_command_list(cl);
        }

        // Destroying the renderer releases the images too.
        r.destroy();
        CT_REQUIRE(r.layer_count(1) == 0);
        rendering::destroy_device(dev);
    }
} // namespace

int main()
{
    test_create_rejects_bad_arguments();
    test_create_and_destroy();
    test_move_transfers_ownership();
    test_render_refuses_bad_arguments();
    test_empty_batch_is_a_no_op();
    test_buffers_grow_per_slot_and_stay_grown();
    test_clip_outside_viewport_is_skipped();
    test_texture_registry();
    test_layers_need_prepare_and_keep_their_images();
    return 0;
}
