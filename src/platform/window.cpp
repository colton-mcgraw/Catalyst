#include <catalyst/platform/window.hpp>

#include "detail_backend.hpp"

namespace catalyst::platform
{

    window create_window(const window_desc &desc)
    {
        return window{detail::create_window(desc)};
    }

    void destroy_window(window &w) noexcept
    {
        if (!w)
            return;

        detail::destroy_window(w.id());
        w = window{};
    }

    bool is_valid(const window &w) noexcept
    {
        if (!w)
            return false;
        return detail::is_window_valid(w.id());
    }

    native_handle get_native_handle(const window &w) noexcept
    {
        if (!w)
            return {};
        return detail::get_native_handle(w.id());
    }

    math::rect<std::int32_t> client_rect_px(const window &w) noexcept
    {
        if (!w)
            return {{0, 0}, {0, 0}};
        return detail::client_rect_px(w.id());
    }

    float dpi_scale(const window &w) noexcept
    {
        if (!w)
            return 1.0f;
        return detail::dpi_scale(w.id());
    }

    void pump_events() noexcept
    {
        detail::pump_events();
    }

    bool wait_events(std::uint32_t timeout_ms) noexcept
    {
        return detail::wait_events(timeout_ms);
    }

    bool poll_event(event &out) noexcept
    {
        return detail::poll_event(out);
    }

    void set_event_sink(core::event_sink *sink) noexcept
    {
        detail::set_event_sink(sink);
    }

} // namespace catalyst::platform
