#pragma once

#include <catalyst/platform/window.hpp>

namespace catalyst::core
{
	class event_sink;
}

namespace catalyst::platform::detail {

const char* backend_name();

	// Windowing
	window_id create_window(const window_desc &desc);
	void destroy_window(window_id id) noexcept;
	bool is_window_valid(window_id id) noexcept;

	native_handle get_native_handle(window_id id) noexcept;
	math::rect<std::int32_t> client_rect_px(window_id id) noexcept;
	float dpi_scale(window_id id) noexcept;

	void pump_events() noexcept;
	bool wait_events(std::uint32_t timeout_ms) noexcept;
	bool poll_event(event &out) noexcept;
	void set_event_sink(core::event_sink *sink) noexcept;

} // namespace catalyst::platform::detail
