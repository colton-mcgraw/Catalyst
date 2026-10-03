/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief `ui::app`: creation and teardown in dependency order, and the frame loop around one host.
 * @details This file is also the reference for a program that writes its own loop: everything here
 * is public API, and the order of calls in `draw` is the order any loop around a `host` needs.
 */

#include <catalyst/rendering/rendering.hpp>
#include <catalyst/ui/app.hpp>

#include <span>
#include <utility>
#include <vector>

namespace catalyst::ui
{

    namespace
    {
        using clock = std::chrono::steady_clock;

        /** @brief How long a minimised window, or one whose swapchain image could not be acquired, waits between tries.
         */
        constexpr std::uint32_t retry_wait_ms = 16;

        [[nodiscard]] rendering::extent2d client_extent(const platform::window &w) noexcept
        {
            const platform::rect_px client = platform::client_rect_px(w);
            return {static_cast<std::uint32_t>(client.size().x()), static_cast<std::uint32_t>(client.size().y())};
        }
    } // namespace

    const char *to_string(app_stage stage) noexcept
    {
        switch (stage)
        {
        case app_stage::window:
            return "window";
        case app_stage::device:
            return "device";
        case app_stage::swapchain:
            return "swapchain";
        case app_stage::host:
            return "host";
        case app_stage::frame_ring:
            return "frame_ring";
        }
        return "unknown";
    }

    /**
     * @brief Everything the app owns, in one allocation: the platform layer holds a pointer to the bus and
     * the input context, and the frame callback a pointer to this.
     * @details Teardown is explicit in the destructor rather than left to member order, because the order
     * that matters -- GPU idle, then command lists, ring, host, swapchain, device, then the window -- is
     * not the order the members are naturally declared in.
     */
    struct app::state
    {
        explicit state(const app_desc &d) : desc(d), in(bus), clear(d.clear_color) {}

        state(const state &) = delete;
        state &operator=(const state &) = delete;

        ~state()
        {
            if (platform::is_valid(window))
                platform::set_frame_callback(window, nullptr, nullptr);
            close_token.reset();
            if (device)
            {
                rendering::wait_idle(device);
                for (rendering::command_list &list : lists)
                    rendering::destroy_command_list(list);
                lists.clear();
                frames = rendering::frame_ring{};
                host.destroy();
                if (swapchain)
                    rendering::destroy_swapchain(swapchain);
                rendering::destroy_device(device);
            }
            platform::set_event_bus(nullptr);
            platform::set_input_feed(nullptr);
            if (window)
                platform::destroy_window(window);
        }

        app_desc desc;
        events::bus bus;
        input::context in;
        events::scoped_token close_token;

        platform::window window{};
        rendering::device device{};
        rendering::swapchain swapchain{};
        rendering::swapchain_desc sd{};
        rendering::queue graphics{};
        rendering::frame_ring frames{};
        std::vector<rendering::command_list> lists; ///< One per slot, created the first time the slot comes round.
        ui::host host;

        color clear{};
        float zoom = 1.0f;
        bool running = true;
        bool redraw = true;     ///< Wait mode: draw the next frame whatever changed. The first frame always draws.
        bool minimized = false; ///< The client area was empty at the last draw.
        bool drawing = false;   ///< `draw` is on the stack; the frame callback must not re-enter it.

        frame_timings timings{};

        /**
         * @brief Folds one host update into the frame's totals. A frame can hold several draws: the frame
         * callback may draw from inside `begin_frame`'s pump (the window's first paint, every step of a
         * resize drag) before `end_frame` draws again, and the program should see all of their work.
         */
        static void add(host_timings &into, const host_timings &t) noexcept
        {
            into.layout += t.layout;
            into.paint += t.paint;
            into.atlas += t.atlas;
            into.laid_out = into.laid_out || t.laid_out;
            into.paints += t.paints;
            into.atlas_uploaded = into.atlas_uploaded || t.atlas_uploaded;
        }

        /** @brief The frame callback: the OS wants a frame now, from inside its modal size/move loop. */
        static void on_frame_request(const platform::window &, void *user) { static_cast<state *>(user)->draw(true); }

        /**
         * @brief Follows the window, updates the host, and draws and presents if there is anything to show.
         * @param forced The OS asked for this frame, so draw it even under wait mode with nothing new.
         * @return False only on a fatal failure, which also ends the loop.
         */
        bool draw(bool forced)
        {
            if (drawing)
                return true;
            drawing = true;
            const struct reset_on_exit
            {
                bool &flag;
                ~reset_on_exit() { flag = false; }
            } guard{drawing};

            // A minimised window has no client area to lay out against or present to.
            const rendering::extent2d client = client_extent(window);
            if (client.width == 0 || client.height == 0)
            {
                minimized = true;
                if (!forced && desc.loop == loop_mode::poll)
                    (void)platform::wait_events(retry_wait_ms);
                return true;
            }
            if (std::exchange(minimized, false))
                redraw = true; // What was presented before minimising is not guaranteed to come back.

            bool resized = false;
            if (client != sd.extent && rendering::resize_swapchain(swapchain, client))
            {
                sd = rendering::get_swapchain_desc(swapchain);
                resized = true;
            }

            host.set_viewport(extent{static_cast<float>(sd.extent.width), static_cast<float>(sd.extent.height)},
                              platform::dpi_scale(window) * zoom);
            const bool changed = host.update();
            add(timings.ui, host.timings());

            if (desc.loop == loop_mode::wait && !changed && !resized && !redraw && !forced)
                return true;
            redraw = false;

            auto t = clock::now();
            auto f = frames.begin(); // Waits for this slot's previous frame, which frees its buffers.
            timings.wait += clock::now() - t;
            if (!f)
            {
                running = false;
                return false;
            }

            while (lists.size() <= f->slot())
                lists.push_back(rendering::create_command_list(f->pool(), "ui_app frame"));
            const rendering::command_list cl = lists[f->slot()];

            t = clock::now();
            rendering::texture back_buffer = rendering::acquire_next_image(swapchain);
            timings.acquire += clock::now() - t;
            if (!back_buffer)
            {
                // Out of date: the next draw's size check recreates it. Nothing was submitted.
                f->end();
                redraw = true;
                if (!forced)
                    (void)platform::wait_events(retry_wait_ms);
                return true;
            }

            const rendering::color_attachment color_target{
                .target = back_buffer,
                .load = rendering::load_op::clear,
                .store = rendering::store_op::store,
                .clear = {clear.r, clear.g, clear.b, clear.a},
            };

            t = clock::now();
            rendering::begin_recording(cl);
            // Layers (group opacity) are drawn offscreen before the pass, and the batch is uploaded there.
            const bool prepared = host.prepare(cl, f->slot());
            rendering::begin_render_pass(cl, {.color_attachments = std::span{&color_target, 1}, .debug_name = "ui"});
            if (prepared)
                (void)host.render(cl, f->slot());
            rendering::end_render_pass(cl);
            rendering::end_recording(cl);
            timings.record += clock::now() - t;

            t = clock::now();
            const auto done = rendering::submit(graphics, cl);
            timings.submit += clock::now() - t;
            if (!done)
            {
                f->end();
                if (rendering::is_fatal(done.error().code))
                {
                    running = false;
                    return false;
                }
                redraw = true;
                return true;
            }

            t = clock::now();
            rendering::present(swapchain);
            f->end(*done);
            rendering::pump(device); // Retires what the GPU has finished with, including a replaced glyph atlas.
            timings.present += clock::now() - t;
            timings.drawn = true;
            return true;
        }
    };

    std::expected<app, app_error> app::create(const app_desc &desc)
    {
        auto s = std::make_unique<state>(desc);

        // The bus and the feed first, so the window's first events (its initial size, focus) arrive.
        platform::set_input_feed(&s->in);
        platform::set_event_bus(&s->bus);

        s->window = platform::create_window(desc.window);
        if (!s->window)
            return std::unexpected(app_error{app_stage::window, "the platform could not create the window"});

        rendering::device_desc dd;
        dd.application_name = desc.application_name;
        s->device = rendering::create_device(dd);
        if (!s->device)
            return std::unexpected(app_error{app_stage::device, "no rendering device could be created"});

        rendering::swapchain_desc sd;
        sd.window = platform::get_native_handle(s->window);
        sd.extent = client_extent(s->window);
        sd.debug_name = "ui_app swapchain";
        s->swapchain = rendering::create_swapchain(s->device, sd);
        if (!s->swapchain)
            return std::unexpected(app_error{app_stage::swapchain, "the swapchain could not be created"});
        s->sd = rendering::get_swapchain_desc(s->swapchain); // The backend may have adjusted extent and format.

        auto h = ui::host::create(s->device, s->bus,
                                  {.color_format = s->sd.pixel_format,
                                   .frames_in_flight = desc.frames_in_flight,
                                   .text = desc.text,
                                   .compositing = desc.compositing,
                                   .debug_name = "ui_app"});
        if (!h)
            return std::unexpected(app_error{app_stage::host, h.error().message()});
        s->host = std::move(*h);

        auto ring = rendering::frame_ring::create(s->device,
                                                  {.frames_in_flight = desc.frames_in_flight, .debug_name = "ui_app"});
        if (!ring)
            return std::unexpected(app_error{app_stage::frame_ring, ring.error().message()});
        s->frames = std::move(*ring);
        s->graphics = rendering::get_queue(s->device);

        if (desc.close_quits)
        {
            state *raw = s.get();
            s->close_token = s->bus.add_listener<platform::window_close_requested_event>(
                [raw](const platform::window_close_requested_event &) { raw->running = false; });
        }
        platform::set_frame_callback(s->window, &state::on_frame_request, s.get());

        app a;
        a.state_ = std::move(s);
        return a;
    }

    app::app() noexcept = default;
    app::app(app &&) noexcept = default;
    app &app::operator=(app &&) noexcept = default;
    app::~app() = default;

    platform::window app::window() const noexcept
    {
        return state_->window;
    }
    rendering::device app::device() const noexcept
    {
        return state_->device;
    }
    events::bus &app::bus() noexcept
    {
        return state_->bus;
    }
    input::context &app::input() noexcept
    {
        return state_->in;
    }
    ui::host &app::host() noexcept
    {
        return state_->host;
    }

    void app::set_zoom(float zoom) noexcept
    {
        state_->zoom = zoom;
    }
    float app::zoom() const noexcept
    {
        return state_->zoom;
    }
    void app::set_clear_color(color c) noexcept
    {
        state_->clear = c;
        state_->redraw = true;
    }
    void app::request_redraw() noexcept
    {
        state_->redraw = true;
    }

    void app::request_quit() noexcept
    {
        state_->running = false;
    }
    bool app::running() const noexcept
    {
        return state_ != nullptr && state_->running;
    }

    bool app::begin_frame()
    {
        state &s = *state_;
        if (!s.running || !platform::is_valid(s.window))
            return false;

        s.timings = frame_timings{};
        const auto start = clock::now();
        // Order matters: clear the previous frame's edges, let the window deliver (which drives every
        // listener), poll what has no window, then evaluate.
        s.in.new_frame();
        // wait_events only blocks until something is queued; pump_events is what dispatches it. Waiting
        // without the pump would leave the message queued and every later wait returning at once.
        const bool idle = s.desc.loop == loop_mode::wait && (s.minimized || (!s.redraw && !s.host.pending()));
        if (idle)
            (void)platform::wait_events();
        platform::pump_events();
        s.in.poll();
        s.in.update();
        s.timings.events = clock::now() - start;

        return s.running && platform::is_valid(s.window);
    }

    bool app::end_frame()
    {
        state &s = *state_;
        if (!s.running)
            return false;
        return s.draw(false);
    }

    const frame_timings &app::timings() const noexcept
    {
        return state_->timings;
    }

} // namespace catalyst::ui
