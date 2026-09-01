/**
 * @file detail_backend.hpp
 * @brief Internal header for platform-specific backend implementations. This header declares the interface for the platform-specific implementations of window management and event handling. The actual implementations will be defined in separate source files corresponding to each supported platform (e.g. win32, x11, etc.). This header should not be included directly by users of the Catalyst Platform library; instead, users should include the main platform.hpp header, which provides a stable API for interacting with the platform functionalities.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include <catalyst/platform/window.hpp>
#include <catalyst/platform/monitor.hpp>

#include <memory>

/**
 * @namespace catalyst::core
 * @brief The catalyst::core namespace contains core functionalities of the Catalyst framework, including event handling and dispatching.
 * @details The catalyst::core module provides essential components for the Catalyst framework, such as event handling, dispatching, and other core utilities that are fundamental to the operation of the framework. By organizing these core functionalities within the catalyst::core namespace, we can provide a clear structure for users of the library to access and utilize these essential tools while maintaining separation from other modules such as platform and physics.
 */
namespace catalyst::core
{
	class event_sink; // Forward declaration of event_sink to avoid circular dependency with platform events.
}

/**
 * @namespace catalyst::platform::detail
 * @brief The catalyst::platform::detail namespace contains internal implementation details for the Catalyst Platform library. This includes platform-specific backend implementations for window management and event handling. The functions declared in this namespace are intended to be used by the public API of the Catalyst Platform library and should not be called directly by users of the library. By organizing these implementation details within a nested detail namespace, we can maintain a clear separation between the public API and the internal workings of the library, allowing for easier maintenance and potential future changes to the implementation without affecting users of the library.
 * @details The catalyst::platform::detail namespace is an internal namespace that contains the platform-specific implementations for window management and event handling. This includes functions for creating and destroying windows, retrieving native handles, managing event sinks, and other platform-specific operations. The actual implementations of these functions will be defined in separate source files corresponding to each supported platform (e.g. win32, x11, etc.). Users of the Catalyst Platform library should not include this header directly or call these functions directly; instead, they should use the public API provided by the main platform.hpp header, which will internally call these detail functions as needed to perform the required operations on the underlying platform.
 */
namespace catalyst::platform::detail
{

	/**
	 * @fn backend_name
	 * @brief Returns the name of the platform backend being used. This function provides a way to identify which platform-specific implementation is currently active, such as "win32" for the Windows platform. This can be useful for debugging, logging, or conditional behavior based on the platform. The returned string is typically a compile-time constant that indicates the specific backend in use.
	 * @return A string representing the name of the platform backend being used. This can be used for debugging, logging, or conditional behavior based on the platform.
	 */
	const char *backend_name();

	/**
	 * @fn create_window
	 * @brief Creates a window with the specified description and returns its unique identifier. This function is responsible for creating a window on the underlying platform based on the provided window_desc structure, which contains parameters such as title, width, height, visibility, and resizability. The function returns a window_id that can be used to reference and manage the created window in subsequent operations.
	 * @param desc A structure containing the description of the window to be created, including parameters such as title, width, height, visibility, and resizability.
	 * @return A unique identifier (window_id) for the created window, which can be used to reference and manage the window in subsequent operations.
	 */
	window_id create_window(const window_desc &desc);
	/**
	 * @fn destroy_window
	 * @brief Destroys the window associated with the given window_id. This function is responsible for properly cleaning up and releasing any resources associated with the specified window. After calling this function, the window_id should no longer be considered valid, and any further operations on that window_id may result in undefined behavior.
	 * @param id The unique identifier (window_id) of the window to be destroyed. This identifier should have been obtained from a previous call to create_window.
	 */
	void destroy_window(window_id id) noexcept;
	/**
	 * @fn is_window_valid
	 * @brief Checks if the given window_id corresponds to a valid and existing window. This function can be used to verify whether a window with the specified window_id has been successfully created and has not been destroyed. It returns true if the window_id is valid and corresponds to an existing window, and false otherwise.
	 * @param id The unique identifier (window_id) of the window to check for validity. This identifier should have been obtained from a previous call to create_window.
	 * @return True if the window_id corresponds to a valid and existing window, false otherwise.
	 */
	bool is_window_valid(window_id id) noexcept;
	/**
	 * @fn get_native_handle
	 * @brief Retrieves the native handle associated with the specified window_id. This function returns a native_handle structure that contains information about the type of native handle and the actual handle value, which can be used for platform-specific operations or interfacing with native APIs. The returned native_handle may vary depending on the underlying platform and the type of window being managed.
	 * @param id The unique identifier (window_id) of the window for which to retrieve the native handle. This identifier should have been obtained from a previous call to create_window.
	 * @return A native_handle structure containing information about the type of native handle and the actual handle value associated with the specified window_id.
	 */
	native_handle get_native_handle(window_id id) noexcept;
	/**
	 * @fn client_rect_px
	 * @brief Retrieves the client area rectangle of the specified window in pixels. This function returns a rect structure that defines the dimensions of the client area of the window, which is the area available for rendering content. The dimensions are typically given in pixels and can be used for layout calculations, rendering, or other operations that require knowledge of the window's client area size.
	 * @param id The unique identifier (window_id) of the window for which to retrieve the client area rectangle. This identifier should have been obtained from a previous call to create_window.
	 * @return A rect structure representing the dimensions of the client area of the specified window in pixels.
	 */
	math::rect<std::int32_t> client_rect_px(window_id id) noexcept;
	/**
	 * @fn dpi_scale
	 * @brief Retrieves the DPI scaling factor for the specified window. This function returns a float value representing the scaling factor that should be applied to convert between logical units (e.g. device-independent pixels) and physical pixels for the given window. The DPI scaling factor can be used to ensure that UI elements and content are rendered at the appropriate size on high-DPI displays, providing a consistent user experience across different screen resolutions and densities.
	 * @param id The unique identifier (window_id) of the window for which to retrieve the DPI scaling factor. This identifier should have been obtained from a previous call to create_window.
	 * @return A float value representing the DPI scaling factor for the specified window, which can be used to convert between logical units and physical pixels.
	 */
	float dpi_scale(window_id id) noexcept;
	/**
	 * @fn pump_events
	 * @brief Pumps the event queue, processing any pending events and dispatching them to the appropriate handlers. This function should be called regularly (e.g. once per frame) to ensure that events are processed in a timely manner and that the application remains responsive to user input and other events. The implementation of this function will typically involve retrieving events from the underlying platform's event system, translating them into the library's event format, and then publishing them through the event_sink for handling by subscribed handlers.
	 */
	void pump_events() noexcept;
	/**
	 * @fn wait_events
	 * @brief Waits for events to occur, with an optional timeout. This function blocks until at least one event is available in the event queue or until the specified timeout has elapsed. If events are available, they will be processed and dispatched to the appropriate handlers. The timeout parameter allows the function to return after a certain amount of time if no events have occurred, preventing indefinite blocking. This can be useful for applications that want to perform periodic updates even when no user input or other events are occurring.
	 * @param timeout_ms The maximum amount of time to wait for events, in milliseconds. If this parameter is zero, the function will block indefinitely until an event occurs. If this parameter is non-zero, the function will return after the specified amount of time if no events have occurred.
	 * @return True if events were processed, false if the timeout elapsed without any events occurring.
	 */
	bool wait_events(std::uint32_t timeout_ms) noexcept;
	/**
	 * @fn poll_event
	 * @brief Polls for a single event from the event queue, if available. This function checks if there are any pending events in the event queue and retrieves one event if it is available. If an event is retrieved, it is stored in the provided output parameter and the function returns true. If no events are available, the function returns false. This allows applications to process events one at a time without blocking, which can be useful for certain types of applications or for implementing custom event loops.
	 * @param out An output parameter that will be set to point to the retrieved event if one is available. The caller is responsible for managing the memory of the retrieved event (e.g. by using a unique_ptr).
	 * @return True if an event was retrieved and stored in the output parameter, false if no events were available.
	 */
	bool poll_event(std::unique_ptr<core::event_base> &out) noexcept;
	/**
	 * @fn set_event_sink
	 * @brief Sets the event_sink instance that will be used for publishing events generated by the window system. This function allows the platform-specific implementation to have a reference to the event_sink, which is responsible for managing event subscriptions and dispatching events to handlers. By setting the event_sink, the platform implementation can publish events through it whenever events are generated in response to window messages or user interactions, allowing subscribed handlers to receive and process the events accordingly.
	 * @param sink A pointer to an event_sink instance that will be used for publishing events generated by the window system. This pointer should typically point to a valid event_sink instance that is responsible for managing event subscriptions and dispatching events to handlers.
	 */
	void set_event_sink(core::event_sink *sink) noexcept;
	void set_cursor_mode(window_id id, cursor_mode mode) noexcept;
	[[nodiscard]] cursor_mode get_cursor_mode(window_id id) noexcept;

	// ----------------------
	// Monitor backend API
	// ----------------------

	[[nodiscard]] std::size_t get_monitor_count() noexcept;
	[[nodiscard]] std::vector<monitor_desc> get_monitor_list() noexcept;
	[[nodiscard]] monitor_desc get_monitor(monitor_id id) noexcept;
	[[nodiscard]] monitor_id primary_monitor() noexcept;
	[[nodiscard]] monitor_id monitor_for_window(window_id w) noexcept;

} // namespace catalyst::platform::detail
