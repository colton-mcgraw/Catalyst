/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The ready-made shell: a window, a device and a frame loop around one `ui::host`.
 * @details For a program whose window is all UI -- a tool, a launcher, a sample -- `app` is the whole
 * of the setup: it creates the window, the event bus and input context, the device, swapchain and
 * frame ring, and a host sized to the window, and runs the frame. The program builds its tree
 * under `root()`, listens for the UI events it cares about on `bus()`, and calls `run`.
 *
 * It is composition, not a base class. Nothing is overridden; the program's code runs between
 * `begin_frame` and `end_frame`, or in the callback `run` takes, and reacts to events through bus
 * listeners like any other code. Everything `app` does goes through public API -- `host`, the
 * platform, input and rendering modules -- so a program that needs its own scene under the UI, a
 * second window or a different present policy keeps its `host` and writes the loop itself;
 * app.cpp is the starting point.
 *
 * What it adds over a hand-written loop:
 *   - Live resizing. It installs a `platform::set_frame_callback`, so the UI keeps redrawing while
 *     the user drags the window's border, instead of freezing until the button is released.
 *   - Scale. The host's scale follows the window's DPI, times a zoom the program can set.
 *   - An idle mode. With `loop_mode::wait` a frame is drawn only when something changed, and
 *     between changes the thread sleeps in `platform::wait_events`: an editor that is not being
 *     touched uses no CPU.
 *
 * Only one `app` may exist at a time: the platform layer has one event bus and one input feed.
 *
 * @code
 *   auto a = ui::app::create({.window = {.title = "Tool"}, .loop = ui::loop_mode::wait});
 *   if (!a) return 1;
 *   build(a->tree(), a->root(), a->text());
 *   auto clicks = a->bus().add_listener<ui::click_event>([&](const ui::click_event &e) { ... });
 *   a->run();
 * @endcode
 */

#pragma once

#include <catalyst/events/bus.hpp>
#include <catalyst/input/context.hpp>
#include <catalyst/platform/window.hpp>
#include <catalyst/rendering/device.hpp>
#include <catalyst/ui/color.hpp>
#include <catalyst/ui/host.hpp>

#include <chrono>
#include <concepts>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>

namespace catalyst::ui
{

    /**
     * @enum loop_mode
     * @brief How `begin_frame` waits and whether `end_frame` draws when nothing changed.
     */
    enum class loop_mode : std::uint8_t
    {
        /** @brief Never wait; draw every frame. For anything animated, and for games. */
        poll,
        /**
         * @brief Sleep until an event arrives unless a redraw is pending, and draw only when the host's
         * batch changed, the window resized, or `request_redraw` was called. The OS keeps showing the
         * last presented image in between.
         */
        wait,
    };

    /**
     * @struct app_desc
     * @brief What the shell creates.
     */
    struct app_desc
    {
        /** @brief The window. Its size is in whatever `ui::length` units it gives; `dp` follows the monitor. */
        platform::window_desc window{};
        /** @brief Reported to the graphics API as the application name. */
        const char *application_name = "catalyst_ui_app";
        /** @brief Frames in flight for the frame ring and the host. */
        std::uint32_t frames_in_flight = 2;
        /** @brief What the window is cleared to under the UI, linear like every `ui::color`. */
        color clear_color = color::rgba(0.0f, 0.0f, 0.0f, 1.0f);
        /** @brief The font, as `host_desc::text`. Not owned; must outlive the app. */
        text_provider *text = nullptr;
        /** @brief As `host_desc::compositing`. */
        opacity_mode compositing = opacity_mode::group;
        /** @brief Poll or wait; see `loop_mode`. */
        loop_mode loop = loop_mode::poll;
        /**
         * @brief End the loop when the user asks to close the window.
         * @details Off for a program that wants to ask first (unsaved changes): it listens for
         * `platform::window_close_requested_event` itself and calls `request_quit` when it agrees.
         */
        bool close_quits = true;
    };

    /**
     * @enum app_stage
     * @brief The part of `app::create` that failed.
     */
    enum class app_stage : std::uint8_t
    {
        window,
        device,
        swapchain,
        host,
        frame_ring,
    };

    /**
     * @struct app_error
     * @brief Why `app::create` failed.
     */
    struct app_error
    {
        app_stage stage = app_stage::window;
        std::string message;
    };

    /** @brief The stage's name, for a log line. */
    [[nodiscard]] const char *to_string(app_stage stage) noexcept;

    /**
     * @struct frame_timings
     * @brief Where the CPU time went between one `begin_frame` and the end of the following `end_frame`.
     * @details Sums, not the last draw: the frame callback can draw from inside `begin_frame` (the
     * window's first paint, each step of a resize drag) before `end_frame` draws, so one loop
     * iteration may hold several draws, and `ui.laid_out` or `ui.atlas_uploaded` say whether any of
     * them did that work. The GPU shows up only as `wait` (the frame ring blocking until the slot's
     * previous frame finished) and as `present` blocking. When nothing was drawn, `drawn` is false
     * and only `events` and `ui` are filled in.
     */
    struct frame_timings
    {
        using duration = std::chrono::steady_clock::duration;

        /** @brief Pumping or waiting for window events and updating input, which runs every listener they drive. */
        duration events{};
        /** @brief What the host's `update` calls did, summed; see `host_timings`. */
        host_timings ui{};
        /** @brief `frame_ring::begin`. */
        duration wait{};
        /** @brief `acquire_next_image`. */
        duration acquire{};
        /** @brief Recording the command list, including the host's upload and layer passes. */
        duration record{};
        /** @brief `submit`. */
        duration submit{};
        /** @brief `present`, ending the frame and `rendering::pump`. */
        duration present{};
        /** @brief Whether at least one frame was drawn and presented. */
        bool drawn = false;
    };

    /**
     * @class app
     * @brief A window drawing one `host`, and the loop that drives it.
     * @details Move-only; its state is one allocation, because the platform layer and the listeners
     * keep pointers into it. Destroying it waits for the GPU and tears everything down in order.
     */
    class app
    {
    public:
        /**
         * @brief Creates the window, bus, input context, device, swapchain, host and frame ring.
         * @return The app, or which step failed and why.
         */
        [[nodiscard]] static std::expected<app, app_error> create(const app_desc &desc = {});

        /** @brief An invalid app. */
        app() noexcept;
        app(const app &) = delete;
        app &operator=(const app &) = delete;
        app(app &&) noexcept;
        app &operator=(app &&) noexcept;
        ~app();

        [[nodiscard]] bool valid() const noexcept { return state_ != nullptr; }
        [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

        // ---- the parts ------------------------------------------------------------------------------

        [[nodiscard]] platform::window window() const noexcept;
        [[nodiscard]] rendering::device device() const noexcept;
        /** @brief The one bus: window events, input events and UI events all arrive here. */
        [[nodiscard]] events::bus &bus() noexcept;
        /** @brief The input context the window feeds. Its state is current between `begin_frame` and `end_frame`. */
        [[nodiscard]] input::context &input() noexcept;
        [[nodiscard]] ui::host &host() noexcept;

        /** @brief Shorthand for `host().tree()`. */
        [[nodiscard]] ui::tree &tree() noexcept { return host().tree(); }
        /** @brief Shorthand for `host().root()`. */
        [[nodiscard]] node root() noexcept { return host().root(); }
        /** @brief Shorthand for `host().text()`. */
        [[nodiscard]] text_provider &text() noexcept { return host().text(); }

        // ---- presentation ---------------------------------------------------------------------------

        /** @brief Multiplies the window's DPI scale. 1 by default; the next frame lays out at the new scale. */
        void set_zoom(float zoom) noexcept;
        [[nodiscard]] float zoom() const noexcept;

        void set_clear_color(color c) noexcept;

        /** @brief Under `loop_mode::wait`, draws the next frame even if nothing changed. No effect under `poll`. */
        void request_redraw() noexcept;

        // ---- the loop -------------------------------------------------------------------------------

        /** @brief Makes the next `begin_frame` return false. */
        void request_quit() noexcept;
        /** @brief False once `request_quit` was called, the window closed, or drawing failed fatally. */
        [[nodiscard]] bool running() const noexcept;

        /**
         * @brief Delivers this frame's events: waits for them under `loop_mode::wait` when nothing is
         * pending, then pumps the window and updates input, which runs every listener on the bus.
         * @return False when the loop should end. The program's per-frame code goes after a true return.
         */
        bool begin_frame();

        /**
         * @brief Follows the window's size and scale, updates the host and, unless the window is
         * minimised or the wait mode has nothing new to show, draws and presents.
         * @return False when drawing failed fatally, which also ends `running()`.
         */
        bool end_frame();

        /** @brief Runs the loop until it ends. */
        void run()
        {
            while (begin_frame() && end_frame())
            {
            }
        }

        /** @brief Runs the loop until it ends, calling `per_frame()` between `begin_frame` and `end_frame`. */
        template <std::invocable F>
        void run(F &&per_frame)
        {
            while (begin_frame())
            {
                per_frame();
                if (!end_frame())
                    break;
            }
        }

        /** @brief Where the last frame's time went. */
        [[nodiscard]] const frame_timings &timings() const noexcept;

    private:
        struct state;
        std::unique_ptr<state> state_;
    };

} // namespace catalyst::ui
