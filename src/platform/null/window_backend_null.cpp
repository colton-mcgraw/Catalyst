#include <catalyst/platform/window.hpp>

#include <catalyst/math/rect.hpp>

#include <catalyst/core/event_sink.hpp>

#include <deque>
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
        };

        std::uint64_t g_next_window_id = 1;
        std::unordered_map<window_id, window_state> g_windows;
        std::deque<event> g_events;

        void enqueue_created_events(window_id id, const window_state &s)
        {
            event e;
            e.window = id;
            e.type = event_type::window_resized;
            e.resized.width_px = s.rect_px.size.x;
            e.resized.height_px = s.rect_px.size.y;
            g_events.push_back(e);

            event dpi;
            dpi.window = id;
            dpi.type = event_type::window_dpi_changed;
            dpi.dpi.dpi_scale = s.dpi_scale;
            g_events.push_back(dpi);
        }
    }

    window_id create_window(const window_desc &desc)
    {
        window_state s;
        s.rect_px.min = {0, 0};
        s.rect_px.max = {desc.width_px, desc.height_px};

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

        event e;
        e.window = id;
        e.type = event_type::window_destroyed;
        g_events.push_back(e);
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

    bool poll_event(event &out) noexcept
    {
        if (g_events.empty())
            return false;

        out = g_events.front();
        g_events.pop_front();
        return true;
    }

    void set_event_sink(core::event_sink *sink) noexcept
    {
        (void)sink;
    }

} // namespace catalyst::platform::detail
