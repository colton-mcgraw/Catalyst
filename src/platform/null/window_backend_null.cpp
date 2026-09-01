#include <catalyst/platform/window.hpp>

#include <catalyst/core/event_sink.hpp>

#include <deque>
#include <memory>
#include <unordered_map>

#include <chrono>
#include <thread>

namespace catalyst::platform::detail
{

    namespace
    {
        struct window_state
        {
            catalyst::math::rect<std::int32_t> rect_px{{0, 0}, {0, 0}};
            float dpi_scale = 1.0f;
            cursor_mode cursor = cursor_mode::normal;
        };

        std::uint64_t g_next_window_id = 1;
        std::unordered_map<window_id, window_state> g_windows;
        std::deque<std::unique_ptr<core::event_base>> g_events;

        core::event_sink *g_event_sink = nullptr;

        void enqueue_event(std::unique_ptr<core::event_base> e)
        {
            if (!e)
                return;

            e->stamp();

            // Exactly one delivery path: sink if installed, otherwise the poll_event() queue (see win32 backend).
            if (g_event_sink)
            {
                g_event_sink->publish(*e);
                return;
            }

            g_events.push_back(std::move(e));
        }

        template <typename E>
        void enqueue_event(E e)
        {
            enqueue_event(std::make_unique<E>(std::move(e)));
        }

        std::int32_t resolve_px(const ui::length &v, ui::axis a, float dpi_scale) noexcept
        {
            ui::resolve_context ctx{};
            ctx.dpi_scale = dpi_scale;
            ctx.dpi_x = dpi_scale * 96.0f;
            ctx.dpi_y = dpi_scale * 96.0f;
            const float px = ui::resolve_or(v, a, ctx, 0.0f);
            return static_cast<std::int32_t>(px);
        }

        void enqueue_created_events(window_id id, const window_state &s)
        {
            window_resized_event e;
            e.window = id;
            e.width_px = s.rect_px.size().x;
            e.height_px = s.rect_px.size().y;
            enqueue_event(e);

            window_dpi_changed_event dpi;
            dpi.window = id;
            dpi.dpi_scale = s.dpi_scale;
            enqueue_event(dpi);
        }
    }

    window_id create_window(const window_desc &desc)
    {
        window_state s;
        s.rect_px.min = {0, 0};
        s.rect_px.max = {resolve_px(desc.width_px, ui::axis::x, s.dpi_scale),
                         resolve_px(desc.height_px, ui::axis::y, s.dpi_scale)};

        const window_id id = g_next_window_id++;
        g_windows.emplace(id, s);
        enqueue_created_events(id, s);
        return id;
    }

    void destroy_window(window_id id) noexcept
    {
        auto it = g_windows.find(id);
        if (it == g_windows.end())
            return;

        g_windows.erase(it);

        window_destroyed_event e;
        e.window = id;
        enqueue_event(e);
    }

    bool is_window_valid(window_id id) noexcept
    {
        return g_windows.find(id) != g_windows.end();
    }

    native_handle get_native_handle(window_id /*id*/) noexcept
    {
        return {};
    }

    catalyst::math::rect<std::int32_t> client_rect_px(window_id id) noexcept
    {
        auto it = g_windows.find(id);
        if (it == g_windows.end())
            return {{0, 0}, {0, 0}};

        return it->second.rect_px;
    }

    float dpi_scale(window_id id) noexcept
    {
        auto it = g_windows.find(id);
        if (it == g_windows.end())
            return 1.0f;

        return it->second.dpi_scale;
    }

    void pump_events() noexcept
    {
        // No OS.
    }

    bool wait_events(std::uint32_t timeout_ms) noexcept
    {
        if (!g_events.empty())
            return true;
        if (timeout_ms == 0)
            return false;

        if (timeout_ms == 0xFFFFFFFFu)
        {
            while (g_events.empty())
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
        return !g_events.empty();
    }

    bool poll_event(std::unique_ptr<core::event_base> &out) noexcept
    {
        if (g_events.empty())
            return false;

        out = std::move(g_events.front());
        g_events.pop_front();
        return true;
    }

    void set_event_sink(core::event_sink *sink) noexcept
    {
        g_event_sink = sink;
    }

    void set_cursor_mode(window_id id, cursor_mode mode) noexcept
    {
        auto it = g_windows.find(id);
        if (it == g_windows.end())
            return;
        it->second.cursor = mode;
    }

    cursor_mode get_cursor_mode(window_id id) noexcept
    {
        auto it = g_windows.find(id);
        if (it == g_windows.end())
            return cursor_mode::normal;
        return it->second.cursor;
    }

} // namespace catalyst::platform::detail
