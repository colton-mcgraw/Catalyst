
#include <catalyst/platform/monitor.hpp>

#include "detail_backend.hpp"

namespace catalyst::platform
{
	std::size_t get_monitor_count() noexcept
	{
		return detail::get_monitor_count();
	}

	std::vector<monitor_desc> get_monitor_list() noexcept
	{
		return detail::get_monitor_list();
	}

	monitor_desc get_monitor(monitor_id id) noexcept
	{
		return detail::get_monitor(id);
	}

	monitor_id primary_monitor() noexcept
	{
		return detail::primary_monitor();
	}

	monitor_id monitor_for_window(const window &w) noexcept
	{
		if (!w)
			return 0;
		return detail::monitor_for_window(w.id());
	}
} // namespace catalyst::platform

