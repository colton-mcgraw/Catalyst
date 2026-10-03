/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Tests for `ui::host`: when it lays out and paints, how it follows a growing glyph atlas, and that
 * what it records submits.
 * @details These need a rendering device, like the renderer bridge's tests, and take the same
 * CATALYST_RENDERING_VALIDATION switch. The font is a fake that is also its own atlas, so the tests
 * control exactly when the atlas has new pixels and when it grows.
 */

#include <catalyst/events/bus.hpp>
#include <catalyst/input/mouse.hpp>
#include <catalyst/rendering/rendering.hpp>
#include <catalyst/ui/host.hpp>
#include <catalyst/ui/ui.hpp>

#include "../rendering/validation_option.hpp"
#include "../test_common.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

using namespace catalyst;

namespace
{
    constexpr rendering::extent2d k_target{64, 64};
    constexpr rendering::format k_format = rendering::format::rgba8_unorm;
    constexpr ui::texture_key k_atlas_key = 7;

    rendering::device make_device()
    {
        rendering::device dev =
            rendering::create_device(tests::with_validation({.application_name = "catalyst ui host tests"}));
        CT_REQUIRE(rendering::is_valid(dev));
        return dev;
    }

    ui::host make_host(const rendering::device &dev, events::bus &bus, ui::host_desc desc = {})
    {
        desc.color_format = k_format;
        desc.debug_name = "ui host test";
        auto h = ui::host::create(dev, bus, desc);
        CT_REQUIRE(h.has_value());
        return std::move(*h);
    }

    /**
     * @brief A provider that is its own 16x16 atlas. Every glyph lookup writes "pixels"; growth is scripted.
     * @details `grow_in_paint` growths happen during painting: the host discards the growth flag once
     * after layout and before the first paint, and the fake takes that call as the moment painting
     * starts, so the growths it then reports are exactly the ones a paint would have caused.
     */
    class fake_font final : public ui::text_provider, public ui::glyph_atlas
    {
    public:
        int grow_in_paint = 0;

        // ---- text_provider ----
        [[nodiscard]] ui::font_metrics metrics(ui::font_id, float size_px) override
        {
            return {.ascent = size_px * 0.8f, .descent = size_px * 0.2f};
        }

        [[nodiscard]] bool glyph_of(ui::font_id, float size_px, char32_t code_point, ui::glyph &out) override
        {
            out.code_point = code_point;
            out.advance = size_px * 0.6f;
            out.bounds = ui::rect::from_pos_size(ui::point{0.0f, -size_px * 0.8f}, ui::extent{size_px * 0.5f, size_px});
            out.uv = ui::rect::from_pos_size(ui::point{0.0f, 0.0f}, ui::extent{1.0f, 1.0f});
            out.texture = k_atlas_key;
            dirty_ = true;
            if (painting_ && grow_in_paint > 0)
            {
                --grow_in_paint;
                resized_ = true;
            }
            return true;
        }

        [[nodiscard]] ui::glyph_atlas *atlas() noexcept override { return this; }

        // ---- glyph_atlas ----
        [[nodiscard]] ui::texture_key key() const noexcept override { return k_atlas_key; }
        [[nodiscard]] std::span<const std::byte> pixels() const noexcept override { return pixels_; }
        [[nodiscard]] std::uint32_t width() const noexcept override { return 16; }
        [[nodiscard]] std::uint32_t height() const noexcept override { return 16; }
        [[nodiscard]] bool take_dirty() noexcept override { return std::exchange(dirty_, false); }
        [[nodiscard]] bool take_resized() noexcept override
        {
            painting_ = true;
            return std::exchange(resized_, false);
        }

    private:
        std::vector<std::byte> pixels_ = std::vector<std::byte>(16 * 16, std::byte{0xff});
        bool dirty_ = false;
        bool resized_ = false;
        bool painting_ = false;
    };

    /** @brief Puts one line of text under the host's root, set with the host's provider. */
    void add_label(ui::host &h, ui::text_content &content)
    {
        content.text = "Host";
        content.provider = &h.text();
        const ui::node n = h.tree().create_child(h.root());
        content.attach(h.tree(), n);
    }

    const ui::extent k_viewport{static_cast<float>(k_target.width), static_cast<float>(k_target.height)};

    // ---------------------------------------------------------------------------------------------

    void test_create_builds_a_root_that_fills_the_viewport()
    {
        rendering::device dev = make_device();
        events::bus bus;
        {
            ui::host h = make_host(dev, bus);
            CT_REQUIRE(h.valid());
            CT_REQUIRE(!ui::is_null(h.root()));
            CT_REQUIRE(h.interaction().root() == h.root());
            CT_REQUIRE(h.renderer().valid());
            CT_REQUIRE(h.renderer().frames_in_flight() == 2);

            h.set_viewport(k_viewport, 2.0f);
            CT_REQUIRE(h.update());
            const ui::layout_result &r = h.tree().layout_of(h.root());
            CT_REQUIRE(r.size.x() == k_viewport.x());
            CT_REQUIRE(r.size.y() == k_viewport.y());
            CT_REQUIRE(r.font_px == 30.0f); // dp(15) at scale 2.
        }
        rendering::destroy_device(dev);
    }

    void test_create_reports_renderer_errors()
    {
        rendering::device dev = make_device();
        events::bus bus;
        auto h = ui::host::create(dev, bus, {.frames_in_flight = 0});
        CT_REQUIRE(!h.has_value());
        CT_REQUIRE(h.error().code == rendering::error_code::invalid_argument);
        rendering::destroy_device(dev);
    }

    void test_update_works_only_when_something_changed()
    {
        rendering::device dev = make_device();
        events::bus bus;
        {
            ui::host h = make_host(dev, bus);
            h.tree().mutable_style(h.root()).background = ui::colors::red;

            // No viewport yet: nothing to lay out against, and the work stays pending.
            CT_REQUIRE(h.pending());
            CT_REQUIRE(!h.update());
            CT_REQUIRE(h.pending());

            h.set_viewport(k_viewport, 1.0f);
            CT_REQUIRE(h.update());
            CT_REQUIRE(h.timings().laid_out);
            CT_REQUIRE(h.timings().paints == 1);
            CT_REQUIRE(!h.batch().empty());
            CT_REQUIRE(!h.pending());

            // Clean: a dirty check and nothing else.
            CT_REQUIRE(!h.update());
            CT_REQUIRE(!h.timings().laid_out);
            CT_REQUIRE(h.timings().paints == 0);

            // The same viewport is not a change.
            h.set_viewport(k_viewport, 1.0f);
            CT_REQUIRE(!h.pending());

            // A new scale is.
            h.set_viewport(k_viewport, 1.5f);
            CT_REQUIRE(h.pending());
            CT_REQUIRE(h.update());
            CT_REQUIRE(h.timings().laid_out);

            // A style change lays out again.
            h.tree().mutable_style(h.root()).background = ui::colors::blue;
            CT_REQUIRE(h.update());
            CT_REQUIRE(h.timings().laid_out);

            // Compositing and request_paint repaint without a layout.
            h.set_compositing(ui::opacity_mode::multiply);
            CT_REQUIRE(h.compositing() == ui::opacity_mode::multiply);
            CT_REQUIRE(h.update());
            CT_REQUIRE(!h.timings().laid_out);
            CT_REQUIRE(h.timings().paints == 1);

            h.set_compositing(ui::opacity_mode::multiply); // Unchanged: nothing to do.
            CT_REQUIRE(!h.pending());

            h.request_paint();
            CT_REQUIRE(h.update());
            CT_REQUIRE(!h.timings().laid_out);
            CT_REQUIRE(h.timings().paints == 1);
        }
        rendering::destroy_device(dev);
    }

    void test_null_provider_by_default()
    {
        rendering::device dev = make_device();
        events::bus bus;
        {
            ui::host h = make_host(dev, bus);
            CT_REQUIRE(h.text().atlas() == nullptr);
            ui::text_content label;
            add_label(h, label);
            h.set_viewport(k_viewport, 1.0f);
            CT_REQUIRE(h.update());
            CT_REQUIRE(!h.timings().atlas_uploaded);
        }
        rendering::destroy_device(dev);
    }

    void test_atlas_is_uploaded_when_it_changes()
    {
        rendering::device dev = make_device();
        events::bus bus;
        fake_font font;
        {
            ui::host h = make_host(dev, bus, {.text = &font});
            CT_REQUIRE(&h.text() == &font);
            ui::text_content label;
            add_label(h, label);

            h.set_viewport(k_viewport, 1.0f);
            CT_REQUIRE(h.update());
            CT_REQUIRE(h.timings().paints == 1);
            CT_REQUIRE(h.timings().atlas_uploaded);
            const rendering::texture first = h.renderer().texture_of(k_atlas_key);
            CT_REQUIRE(rendering::is_valid(first));
            CT_REQUIRE(rendering::get_texture_desc(first).pixel_format == rendering::format::r8_unorm);

            // Repainting asks for the glyphs again, which the fake reports as new pixels: a new texture.
            h.request_paint();
            CT_REQUIRE(h.update());
            CT_REQUIRE(h.timings().atlas_uploaded);
            const rendering::texture second = h.renderer().texture_of(k_atlas_key);
            CT_REQUIRE(rendering::is_valid(second));
            CT_REQUIRE(second.id() != first.id());
            rendering::wait_idle(dev);
            rendering::pump(dev);
            CT_REQUIRE(!rendering::is_valid(first)); // The host destroyed the texture it replaced.
        }
        rendering::destroy_device(dev);
    }

    void test_atlas_growth_during_paint_repaints()
    {
        rendering::device dev = make_device();
        events::bus bus;
        {
            fake_font font;
            font.grow_in_paint = 1;
            ui::host h = make_host(dev, bus, {.text = &font});
            ui::text_content label;
            add_label(h, label);
            h.set_viewport(k_viewport, 1.0f);
            CT_REQUIRE(h.update());
            CT_REQUIRE(h.timings().paints == 2);
        }
        {
            // An atlas that grows on every paint is repainted a bounded number of times, then drawn as is.
            fake_font font;
            font.grow_in_paint = 1000;
            ui::host h = make_host(dev, bus, {.text = &font});
            ui::text_content label;
            add_label(h, label);
            h.set_viewport(k_viewport, 1.0f);
            CT_REQUIRE(h.update());
            CT_REQUIRE(h.timings().paints == 5);
            CT_REQUIRE(h.timings().atlas_uploaded);
        }
        rendering::destroy_device(dev);
    }

    void test_bridge_follows_the_desc()
    {
        rendering::device dev = make_device();
        events::bus bus;

        input::mouse_move_event move;
        move.position_px = math::vec2<std::int32_t>{10, 10};
        {
            ui::host h = make_host(dev, bus);
            h.set_viewport(k_viewport, 1.0f);
            (void)h.update();
            bus.dispatch(move);
            CT_REQUIRE(h.interaction().hovered() == h.root());
        }
        {
            ui::host h = make_host(dev, bus, {.bridge_input = false});
            h.set_viewport(k_viewport, 1.0f);
            (void)h.update();
            bus.dispatch(move);
            CT_REQUIRE(ui::is_null(h.interaction().hovered()));
        }
        rendering::destroy_device(dev);
    }

    void test_prepare_and_render_record_and_submit()
    {
        rendering::device dev = make_device();
        events::bus bus;
        fake_font font;
        {
            ui::host h = make_host(dev, bus, {.text = &font});
            ui::text_content label;
            add_label(h, label);
            // A translucent child, so the batch has a layer and prepare has offscreen work to do.
            const ui::node faded = h.tree().create_child(h.root());
            h.tree().mutable_style(faded).height = ui::px(10.0f);
            h.tree().mutable_style(faded).background = ui::colors::red;
            h.tree().mutable_style(faded).opacity = 0.5f;
            h.set_viewport(k_viewport, 1.0f);
            CT_REQUIRE(h.update());
            CT_REQUIRE(!h.batch().layers.empty());

            rendering::texture_desc td;
            td.extent = {k_target.width, k_target.height, 1};
            td.pixel_format = k_format;
            td.usage = rendering::texture_usage::render_target;
            td.debug_name = "ui host test target";
            rendering::texture target = rendering::create_texture(dev, td);
            CT_REQUIRE(rendering::is_valid(target));

            for (std::uint32_t slot = 0; slot < 2; ++slot)
            {
                rendering::command_list cl = rendering::create_command_list(dev, {.debug_name = "ui host test"});
                CT_REQUIRE(rendering::begin_recording(cl));
                CT_REQUIRE(h.prepare(cl, slot));
                const rendering::color_attachment color{
                    .target = target, .load = rendering::load_op::clear, .store = rendering::store_op::store};
                rendering::begin_render_pass(cl, {.color_attachments = std::span{&color, 1}, .debug_name = "ui"});
                CT_REQUIRE(h.render(cl, slot));
                rendering::end_render_pass(cl);
                CT_REQUIRE(rendering::end_recording(cl));
                const auto done = rendering::submit(rendering::get_queue(dev), cl);
                CT_REQUIRE(done.has_value());
                rendering::wait_idle(dev);
                rendering::pump(dev);
                rendering::destroy_command_list(cl);
            }
            rendering::destroy_texture(target);
        }
        rendering::destroy_device(dev);
    }

    void test_move_and_destroy()
    {
        rendering::device dev = make_device();
        events::bus bus;
        {
            ui::host a = make_host(dev, bus);
            const ui::node root = a.root();
            ui::host b = std::move(a);
            CT_REQUIRE(!a.valid());
            CT_REQUIRE(b.valid());
            CT_REQUIRE(b.root() == root);

            // The interaction and the bridge moved with it: input still reaches the tree.
            b.set_viewport(k_viewport, 1.0f);
            (void)b.update();
            input::mouse_move_event move;
            move.position_px = math::vec2<std::int32_t>{5, 5};
            bus.dispatch(move);
            CT_REQUIRE(b.interaction().hovered() == root);

            b.destroy();
            CT_REQUIRE(!b.valid());
            b.destroy();        // Twice is fine.
            bus.dispatch(move); // And nothing is listening any more.

            ui::host c;
            CT_REQUIRE(!c.valid());
        }
        rendering::destroy_device(dev);
    }
} // namespace

int main()
{
    test_create_builds_a_root_that_fills_the_viewport();
    test_create_reports_renderer_errors();
    test_update_works_only_when_something_changed();
    test_null_provider_by_default();
    test_atlas_is_uploaded_when_it_changes();
    test_atlas_growth_during_paint_repaints();
    test_bridge_follows_the_desc();
    test_prepare_and_render_record_and_submit();
    test_move_and_destroy();
    return 0;
}
