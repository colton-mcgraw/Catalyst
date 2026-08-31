#include <catalyst/platform/monitor.hpp>

#include <catalyst/platform/window.hpp>

#include <vector>

namespace catalyst::platform::detail
{
    std::size_t get_monitor_count() noexcept
    {
        return 1;
    }

    std::vector<monitor_desc> get_monitor_list() noexcept
    {
        monitor_desc m;
        m.id = 1;
        m.name = "Null Monitor";
        m.bounds_px = {{0, 0}, {1920, 1080}};
        m.work_area_px = m.bounds_px;
        m.size_mm = {0.0f, 0.0f};
        m.dpi_x = 96.0f;
        m.dpi_y = 96.0f;
        m.refresh_rate_hz = 60;
        m.primary = true;

        return {m};
    }

    monitor_desc get_monitor(monitor_id id) noexcept
    {
        if (id != 1)
            return {};
        return get_monitor_list().front();
    }

    monitor_id primary_monitor() noexcept
    {
        return 1;
    }

    monitor_id monitor_for_window(window_id /*w*/) noexcept
    {
        return 1;
    }
} // namespace catalyst::platform::detail
