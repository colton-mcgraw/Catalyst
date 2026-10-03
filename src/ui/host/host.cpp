/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief `ui::host`: layout and paint on change, the glyph atlas upload, and the renderer calls around them.
 */

#include <catalyst/rendering/texture.hpp>
#include <catalyst/ui/host.hpp>
#include <catalyst/ui/input_bridge.hpp>
#include <catalyst/ui/layout.hpp>
#include <catalyst/ui/measurement.hpp>

#include <cmath>
#include <utility>

namespace catalyst::ui
{

    namespace
    {
        using clock = std::chrono::steady_clock;

        /**
         * @brief Repaints allowed after the atlas grows under a paint. Each growth at least doubles one side up
         * to the provider's own limit, so a handful covers any realistic first frame; past that the batch is
         * drawn with whatever coordinates it has and the next change repaints it.
         */
        constexpr int max_atlas_repaints = 4;

        [[nodiscard]] rendering::extent2d to_extent2d(extent e) noexcept
        {
            return {static_cast<std::uint32_t>(std::lround(e.x())), static_cast<std::uint32_t>(std::lround(e.y()))};
        }
    } // namespace

    /**
     * @brief Everything the host owns, in one allocation so that the pointers the interaction and the
     * listeners hold into it survive the host being moved.
     * @details Member order is destruction order in reverse: the bridge goes first, so no listener runs
     * against a half-destroyed interaction, and the tree last.
     */
    struct host::state
    {
        state(const rendering::device &dev, events::bus &b, const host_desc &d)
            : device(dev), bus(&b), desc(d), ux(tree, b)
        {
        }

        rendering::device device{};
        events::bus *bus = nullptr;
        host_desc desc{};

        null_text_provider boxes;
        ui::tree tree;
        node root = null_node;
        ui::interaction ux;
        input_bridge bridge;

        ui::renderer renderer;
        render_batch batch;
        rendering::texture atlas_texture{};

        extent viewport{0.0f, 0.0f};
        float scale = 1.0f;
        opacity_mode compositing = opacity_mode::group;
        bool paint_requested = true; ///< The first update always paints.

        host_timings timings{};

        [[nodiscard]] text_provider &text() noexcept { return desc.text != nullptr ? *desc.text : boxes; }

        void paint_batch()
        {
            const auto start = clock::now();
            batch.clear();
            batch_builder builder(batch, rect::from_pos_size(point{0.0f, 0.0f}, viewport));
            paint_params params = paint_params::for_viewport(viewport, scale);
            params.compositing = compositing;
            ui::paint(tree, root, builder, params);
            timings.paint += clock::now() - start;
            ++timings.paints;
        }

        /** @brief Uploads the atlas when its pixels changed. Returns true when a new texture was registered. */
        bool sync_atlas(glyph_atlas &atlas)
        {
            if (!atlas.take_dirty() || atlas.width() == 0 || atlas.height() == 0)
                return false;
            const auto start = clock::now();

            rendering::texture_desc td;
            td.extent = {atlas.width(), atlas.height(), 1};
            td.pixel_format = rendering::format::r8_unorm; // One channel: the renderer reads it as a coverage mask.
            td.usage = rendering::texture_usage::sampled;
            td.debug_name = desc.debug_name != nullptr ? desc.debug_name : "ui glyph atlas";
            rendering::texture next = rendering::create_texture(device, td, atlas.pixels());
            bool registered = false;
            if (next && renderer.register_texture(atlas.key(), next))
            {
                // The old atlas may still be read by a frame in flight; the rendering module holds the
                // release back until the GPU has passed it, so destroying it now is safe.
                if (atlas_texture)
                    rendering::destroy_texture(atlas_texture);
                atlas_texture = next;
                registered = true;
            }
            else if (next)
            {
                rendering::destroy_texture(next);
            }

            timings.atlas = clock::now() - start;
            timings.atlas_uploaded = registered;
            return registered;
        }

        bool update()
        {
            timings = host_timings{};
            if (viewport.x() <= 0.0f || viewport.y() <= 0.0f)
                return false;

            if (tree.is_dirty(root))
            {
                const auto start = clock::now();
                ui::layout(tree, root, layout_params::for_viewport(viewport, scale));
                timings.layout = clock::now() - start;
                timings.laid_out = true;
                paint_requested = true;
            }
            if (!paint_requested)
                return false;
            paint_requested = false;

            glyph_atlas *atlas = text().atlas();
            // Measuring during layout rasterises glyphs too; any growth then happened before this paint,
            // whose coordinates are therefore current. Only growth during the paint itself needs a repaint.
            if (atlas != nullptr)
                (void)atlas->take_resized();
            paint_batch();
            for (int attempt = 0; atlas != nullptr && attempt < max_atlas_repaints && atlas->take_resized(); ++attempt)
                paint_batch();
            if (atlas != nullptr)
                sync_atlas(*atlas);
            return true;
        }
    };

    std::expected<host, rendering::error> host::create(const rendering::device &dev, events::bus &bus,
                                                       const host_desc &desc)
    {
        auto r = ui::renderer::create(dev, {.color_format = desc.color_format,
                                            .frames_in_flight = desc.frames_in_flight,
                                            .debug_name = desc.debug_name});
        if (!r)
            return std::unexpected(r.error());

        host h;
        h.state_ = std::make_unique<state>(dev, bus, desc);
        state &s = *h.state_;
        s.renderer = std::move(*r);
        s.compositing = desc.compositing;

        s.root = s.tree.create();
        style &st = s.tree.mutable_style(s.root);
        st.direction = flex_direction::column;
        // Fill the viewport. An auto-sized root is only as tall as its content, so a child with
        // flex_grow would have nothing to grow into.
        st.width = vw(100.0f);
        st.height = vh(100.0f);
        // The default text size, in dp so that it follows the scale. Left auto, text would fall back
        // to 16 raw pixels, which no scale reaches.
        st.font_size = dp(15.0f);
        s.ux.set_root(s.root);

        if (desc.bridge_input)
            s.bridge = input_bridge(bus, s.ux);
        return h;
    }

    host::host() noexcept = default;
    host::host(host &&) noexcept = default;
    host &host::operator=(host &&other) noexcept
    {
        if (this != &other)
        {
            destroy();
            state_ = std::move(other.state_);
        }
        return *this;
    }
    host::~host()
    {
        destroy();
    }

    void host::destroy() noexcept
    {
        if (!state_)
            return;
        state &s = *state_;
        s.bridge = input_bridge{};
        // The renderer goes first: it holds the atlas registered, and does not own it.
        s.renderer.destroy();
        if (s.atlas_texture)
            rendering::destroy_texture(s.atlas_texture);
        state_.reset();
    }

    ui::tree &host::tree() noexcept
    {
        return state_->tree;
    }
    const ui::tree &host::tree() const noexcept
    {
        return state_->tree;
    }
    node host::root() const noexcept
    {
        return state_->root;
    }
    ui::interaction &host::interaction() noexcept
    {
        return state_->ux;
    }
    text_provider &host::text() noexcept
    {
        return state_->text();
    }

    void host::set_viewport(extent viewport_px, float scale) noexcept
    {
        state &s = *state_;
        if (viewport_px.x() == s.viewport.x() && viewport_px.y() == s.viewport.y() && scale == s.scale)
            return;
        s.viewport = viewport_px;
        s.scale = scale;
        s.tree.mark_dirty(s.root);
    }

    extent host::viewport() const noexcept
    {
        return state_->viewport;
    }
    float host::scale() const noexcept
    {
        return state_->scale;
    }

    void host::set_compositing(opacity_mode mode) noexcept
    {
        state &s = *state_;
        if (s.compositing == mode)
            return;
        s.compositing = mode;
        s.paint_requested = true;
    }

    opacity_mode host::compositing() const noexcept
    {
        return state_->compositing;
    }
    void host::request_paint() noexcept
    {
        state_->paint_requested = true;
    }

    bool host::pending() const noexcept
    {
        const state &s = *state_;
        return s.paint_requested || s.tree.is_dirty(s.root);
    }

    bool host::update()
    {
        return state_->update();
    }

    bool host::prepare(const rendering::command_list &cl, std::uint32_t slot)
    {
        state &s = *state_;
        return s.renderer.prepare(cl, s.batch, to_extent2d(s.viewport), slot);
    }

    bool host::render(const rendering::command_list &cl, std::uint32_t slot)
    {
        state &s = *state_;
        return s.renderer.render(cl, s.batch, to_extent2d(s.viewport), slot);
    }

    const render_batch &host::batch() const noexcept
    {
        return state_->batch;
    }
    const host_timings &host::timings() const noexcept
    {
        return state_->timings;
    }
    ui::renderer &host::renderer() noexcept
    {
        return state_->renderer;
    }

} // namespace catalyst::ui
