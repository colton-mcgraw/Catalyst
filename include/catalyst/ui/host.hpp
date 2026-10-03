/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief One UI, hosted: the tree, its interaction, the renderer that draws it and the glyph atlas it samples.
 * @details Every application that puts a `catalyst::ui` tree on screen writes the same glue: an
 * interaction over the tree, a bridge from input events into it, a layout and paint whenever
 * something marked the tree dirty, a fresh upload of the glyph atlas whenever painting added
 * glyphs to it, and `prepare` / `render` around the render pass. `host` is that glue and nothing
 * more. It owns no window, device, swapchain or frame loop: the caller says how large the viewport
 * is and at what scale, calls `update` once a frame, and calls `prepare` and `render` from its own
 * command list, so a host drops into any renderer that can give it a colour pass.
 *
 * `ui::app` (ui/app.hpp) is the ready-made loop around a host for programs that have nothing else
 * to draw. Anything `app` does is done through this header's public API, so a program that
 * outgrows it keeps the host and replaces only the loop.
 *
 * @code
 *   auto ui = ui::host::create(dev, bus, {.color_format = sd.pixel_format, .frames_in_flight = 2});
 *   build_screen(ui->tree(), ui->root(), ui->text());
 *   while (running)
 *   {
 *       // ... pump events and update input; the host's bridge forwards them to the tree ...
 *       ui->set_viewport({float(sd.extent.width), float(sd.extent.height)}, platform::dpi_scale(w));
 *       ui->update();                              // lays out and paints only when something changed
 *       auto f = frames.begin();
 *       begin_recording(cl);
 *       ui->prepare(cl, f->slot());               // outside the pass: layers, uploads
 *       begin_render_pass(cl, ...);
 *       ui->render(cl, f->slot());
 *       end_render_pass(cl);
 *       // ... submit, present, f->end(done), pump(dev) ...
 *   }
 * @endcode
 */

#pragma once

#include <catalyst/events/bus.hpp>
#include <catalyst/rendering/command.hpp>
#include <catalyst/rendering/device.hpp>
#include <catalyst/rendering/error.hpp>
#include <catalyst/rendering/types.hpp>
#include <catalyst/ui/batch.hpp>
#include <catalyst/ui/geometry.hpp>
#include <catalyst/ui/interaction.hpp>
#include <catalyst/ui/node.hpp>
#include <catalyst/ui/paint.hpp>
#include <catalyst/ui/renderer.hpp>
#include <catalyst/ui/text.hpp>

#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>

namespace catalyst::ui
{

    /**
     * @struct host_desc
     * @brief What a `host` needs to know up front.
     */
    struct host_desc
    {
        /** @brief The format of the colour attachment `render` draws into. See `renderer_desc::color_format`. */
        rendering::format color_format = rendering::format::bgra8_unorm_srgb;
        /** @brief Frames in flight. Match the frame ring and pass `frame::slot()` to `prepare` and `render`. */
        std::uint32_t frames_in_flight = 2;
        /**
         * @brief The font every text node should be set with, or null for the host's own `null_text_provider`.
         * @details Not owned; it must outlive the host. When it has an `atlas()`, `update` uploads the
         * atlas as an `r8_unorm` texture whenever it changes and registers it with the renderer.
         */
        text_provider *text = nullptr;
        /** @brief How a translucent node is composited. Changeable later with `set_compositing`. */
        opacity_mode compositing = opacity_mode::group;
        /**
         * @brief Construct an `input_bridge` over the bus, so mouse and keyboard events reach the tree.
         * @details Off for an application that feeds `interaction()` itself, from another pointer source
         * or from input it has already filtered.
         */
        bool bridge_input = true;
        /** @brief Name for the renderer's objects and the atlas texture in graphics debuggers. */
        const char *debug_name = nullptr;
    };

    /**
     * @struct host_timings
     * @brief What the last `update` did and how long each part took. CPU time.
     */
    struct host_timings
    {
        using duration = std::chrono::steady_clock::duration;

        /** @brief `ui::layout` over the tree. Zero when the tree was clean. */
        duration layout{};
        /** @brief Every `ui::paint` into the batch, including repaints after the atlas grew. */
        duration paint{};
        /** @brief Uploading the glyph atlas. Zero when it had not changed. */
        duration atlas{};
        /** @brief Whether the tree was laid out. */
        bool laid_out = false;
        /** @brief How many times the batch was painted: zero when nothing changed, more than one after atlas growth. */
        std::uint32_t paints = 0;
        /** @brief Whether a new atlas texture was uploaded and registered. */
        bool atlas_uploaded = false;
    };

    /**
     * @class host
     * @brief A tree with everything needed to put it on screen, short of the window and the frame loop.
     * @details Move-only; its state lives behind one allocation, because the interaction and the input
     * listeners hold pointers into it. The device and the bus must outlive the host.
     *
     * **The tree.** The host creates the root: a column sized `vw(100)` by `vh(100)` so that it fills
     * the viewport, with `font_size` `dp(15)` so text without a size of its own follows the scale.
     * Restyle it freely through `tree().mutable_style(root())`. Interaction is rooted there too.
     *
     * **When it lays out.** `update` lays the tree out when the root is dirty -- any structural or
     * style change marks it, as does a new viewport or scale -- and paints when it laid out, when
     * the compositing mode changed, or when `request_paint` was called (a node's `paint_fn` drawing
     * something different does not mark the tree dirty, since it cannot move anything). Otherwise
     * `update` costs a dirty check and the batch from last time is drawn again.
     *
     * **The atlas.** When the text provider has an atlas, painting may rasterise glyphs into it. After
     * a paint `update` asks the atlas whether it grew -- old texture coordinates are then stale, and
     * the batch is painted again -- and whether its pixels changed, in which case it uploads the
     * whole atlas as a new texture and registers it under the atlas key, destroying the previous
     * one. The rendering module defers that release until the GPU has finished with it.
     */
    class host
    {
    public:
        /**
         * @brief Creates the renderer, the tree and its root, the interaction and, if asked for, the input bridge.
         * @param dev The device to draw with. Must outlive the host.
         * @param bus The bus input arrives on and UI events are published to. Must outlive the host.
         * @param desc Attachment format, frames in flight, the font and the options above.
         * @return The host, or the error `renderer::create` reported.
         */
        [[nodiscard]] static std::expected<host, rendering::error> create(const rendering::device &dev,
                                                                          events::bus &bus, const host_desc &desc = {});

        /** @brief An invalid host. Everything but `valid` and `destroy` requires a valid one. */
        host() noexcept;
        host(const host &) = delete;
        host &operator=(const host &) = delete;
        host(host &&) noexcept;
        host &operator=(host &&) noexcept;
        ~host();

        /** @brief Releases the renderer and the atlas texture and drops the tree, leaving the host invalid. */
        void destroy() noexcept;

        /** @brief True between `create` and `destroy`. */
        [[nodiscard]] bool valid() const noexcept { return state_ != nullptr; }
        [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

        // ---- the tree -------------------------------------------------------------------------------

        /** @brief The tree. Build under `root()`. */
        [[nodiscard]] ui::tree &tree() noexcept;
        [[nodiscard]] const ui::tree &tree() const noexcept;
        /** @brief The root the host created, sized to the viewport. */
        [[nodiscard]] node root() const noexcept;
        /** @brief The interaction over the tree: hover, press, focus and capture. */
        [[nodiscard]] ui::interaction &interaction() noexcept;
        /** @brief The provider to set every `text_content::provider` to: the one in the desc, or the host's own. */
        [[nodiscard]] text_provider &text() noexcept;

        // ---- what to lay out against ----------------------------------------------------------------

        /**
         * @brief Sets the viewport in pixels and the scale `dp` lengths are multiplied by.
         * @details Marks the root dirty when either changes, so the next `update` lays out again. The
         * scale is typically `platform::dpi_scale` times whatever zoom the application offers.
         */
        void set_viewport(extent viewport_px, float scale) noexcept;
        [[nodiscard]] extent viewport() const noexcept;
        [[nodiscard]] float scale() const noexcept;

        /** @brief Switches between group and per-node opacity. Repaints on the next `update` when it changes. */
        void set_compositing(opacity_mode mode) noexcept;
        [[nodiscard]] opacity_mode compositing() const noexcept;

        /** @brief Paints again on the next `update` even if the tree is clean. For `paint_fn` content that changed. */
        void request_paint() noexcept;

        /** @brief True when the next `update` would lay out or paint. */
        [[nodiscard]] bool pending() const noexcept;

        // ---- the frame ------------------------------------------------------------------------------

        /**
         * @brief Lays out and paints if anything changed, then uploads the atlas if painting changed it.
         * @details Call once a frame, after input has been delivered and before `prepare`. A zero-sized
         * viewport skips layout and paint and leaves them pending.
         * @return True when the batch changed, so a program that draws only on change knows to draw.
         */
        bool update();

        /**
         * @brief Records the batch's offscreen work: uploads it and draws its layers. Outside any render pass.
         * @param cl The recording command list.
         * @param slot The frame slot, below `frames_in_flight`.
         * @return False when the renderer refused; see `renderer::prepare`.
         */
        bool prepare(const rendering::command_list &cl, std::uint32_t slot = 0);

        /**
         * @brief Records the batch's draws into the current render pass, whose attachment is the viewport's size.
         * @return False when the renderer refused; see `renderer::render`.
         */
        bool render(const rendering::command_list &cl, std::uint32_t slot = 0);

        // ---- inspection -----------------------------------------------------------------------------

        /** @brief The batch the last paint produced. */
        [[nodiscard]] const render_batch &batch() const noexcept;
        /** @brief What the last `update` did. */
        [[nodiscard]] const host_timings &timings() const noexcept;
        /** @brief The renderer, for registering the application's own textures. */
        [[nodiscard]] ui::renderer &renderer() noexcept;

    private:
        struct state;
        std::unique_ptr<state> state_;
    };

} // namespace catalyst::ui
