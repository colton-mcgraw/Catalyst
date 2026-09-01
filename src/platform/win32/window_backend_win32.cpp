#include <catalyst/platform/window.hpp>

#include <catalyst/core/event_sink.hpp>
#include <catalyst/input/keyboard.hpp>
#include <catalyst/input/mouse.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // For basic Windows API functions and types (e.g. HWND, HMONITOR, GetKeyState, etc.).

#include "win32_helpers.hpp"

#include <deque>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>

#include <windowsx.h> // For GET_XBUTTON_WPARAM macro to process extended mouse button messages.

/**
 * @namespace catalyst::platform::detail
 * @brief The catalyst::platform::detail namespace contains internal implementation details for the Catalyst Platform library's window management on the Win32 platform. This includes helper functions, internal data structures, and other components that are not intended to be exposed to users of the library. By organizing these implementation details within a nested namespace, we can keep them separate from the public API and avoid naming conflicts, while still allowing for efficient and effective management of windows and events on the Win32 platform.
 * @details The catalyst::platform::detail namespace is used to encapsulate the internal workings of the window management system for the Win32 platform. This includes functions for retrieving native handles, managing window IDs and their associated HWNDs, handling DPI scaling, and other platform-specific details that are necessary for the implementation but should not be exposed to users of the library. By keeping these details within a nested namespace, we can maintain a clean separation between the public API and the internal implementation, allowing users to interact with the library without needing to worry about the complexities of the underlying platform-specific code. This also allows for easier maintenance and potential future expansion of the implementation without affecting the public API.
 */
namespace catalyst::platform::detail
{
    /**
     * @namespace
     * @brief An unnamed namespace to hold internal helper functions and variables for the Win32 window management implementation. This namespace is used to encapsulate implementation details that are specific to the Win32 platform and should not be exposed outside of this translation unit. By using an unnamed namespace, we can ensure that these internal components have internal linkage, preventing naming conflicts and ensuring that they are only accessible within this source file.
     * @details The unnamed namespace within catalyst::platform::detail is used to define helper functions, internal data structures, and global variables that are necessary for the implementation of window management on the Win32 platform. This includes functions for retrieving native handles, managing window IDs and their associated HWNDs, handling DPI scaling, and other platform-specific details. By keeping these components within an unnamed namespace, we can maintain a clear separation between the internal implementation and the public API of the Catalyst Platform library, allowing users to interact with the library without needing to worry about the complexities of the underlying platform-specific code.
     */
    namespace
    {
        /**
         * @fn k_window_class_name
         * @brief A constant wide string representing the name of the window class used for creating windows in this platform implementation. This name is registered with the Windows API when the first window is created, and it is used to identify the type of window being created. The window class name must be unique within the application to avoid conflicts with other window classes that may be registered by the application or by third-party libraries. By using a constant string for the window class name, we can ensure that all windows created by this implementation will use the same class and will have consistent behavior and appearance as defined by that class.
         */
        constexpr wchar_t k_window_class_name[] = L"CatalystWindow";

        /**
         * @var g_events
         * @brief A global deque that holds unique pointers to event_base instances representing the events that have been generated and are pending processing. This deque serves as a queue for events that have been created in response to various window messages and user interactions, allowing them to be processed in an orderly manner. The use of unique pointers ensures that the memory for each event is managed automatically, preventing memory leaks and ensuring that events are properly destroyed when they are no longer needed.
         */
        std::deque<std::unique_ptr<core::event_base>> g_events;
        /**
         * @var g_windows
         * @brief A global unordered map that associates window IDs with their corresponding HWND handles. This map is used to keep track of all the windows that have been created in the application, allowing for efficient lookup of the HWND handle based on the window ID. The window ID is a unique identifier assigned to each window when it is created, and it is used as the key in this map to retrieve the corresponding HWND handle when needed (e.g. for sending messages or querying window properties).
         */
        std::unordered_map<window_id, HWND> g_windows;
        /**
         * @var g_window_ids
         * @brief A global unordered map that associates HWND handles with their corresponding window IDs. This map is used to keep track of all the windows that have been created in the application, allowing for efficient lookup of the window ID based on the HWND handle. The HWND handle is a unique identifier assigned by the Windows API when a window is created, and it is used as the key in this map to retrieve the corresponding window ID when needed (e.g. for processing events or managing windows).
         */
        std::uint64_t g_next_window_id = 1;
        /**
         * @var g_event_sink
         * @brief A global pointer to an event_sink instance that is used for publishing events generated by the window system. This pointer is typically initialized to point to a valid event_sink instance that is responsible for managing event subscriptions and dispatching events to handlers. When events are generated in response to window messages or user interactions, they can be published through this event_sink, allowing subscribed handlers to receive and process the events accordingly. The use of a global pointer allows for easy access to the event_sink from various parts of the platform implementation without needing to pass it around as a parameter.
         */
        core::event_sink *g_event_sink = nullptr;
        /**
         * @var g_last_mouse_pos_px
         * @brief A global unordered map that associates window IDs with their last known mouse position in pixels. This map is used to track the last mouse position for each window, allowing for accurate processing of mouse movement events and other interactions that depend on the mouse position. The mouse position is typically updated in response to mouse movement messages, and it can be used to calculate deltas for mouse movement or to determine the current position of the mouse cursor within a specific window.
         */
        std::unordered_map<window_id, math::vec2<std::int32_t>> g_last_mouse_pos_px;
        /**
         * @var g_pending_high_surrogate
         * @brief A thread-local variable that holds a pending high surrogate character when processing UTF-16 input from the Windows API. This variable is used to handle cases where a high surrogate character is received without its corresponding low surrogate, allowing the system to wait for the next input event to complete the surrogate pair before processing the character.
         * @note By using a thread-local variable, we can ensure that this state is maintained separately for each thread that may be processing input events, preventing conflicts and ensuring correct handling of UTF-16 input across multiple threads.
         */
        thread_local wchar_t g_pending_high_surrogate = 0;

        /**
         * @fn dpi_scale_for_window
         * @brief Retrieves the DPI scaling factor for a given window handle (HWND). This function uses the Windows API to query the DPI settings for the specified window, allowing the application to adjust its rendering and layout accordingly to ensure that it appears correctly on high-DPI displays. If the function to retrieve the DPI for a window is not available (e.g. on older versions of Windows), or if it fails to retrieve a valid DPI value, this function will return a default scaling factor of 1.0f, indicating no scaling.
         * @param hwnd The handle to the window for which to retrieve the DPI scaling factor. This is typically obtained from the window creation process or from event handling when processing messages related to a specific window.
         * @return The DPI scaling factor for the specified window. This is calculated based on the DPI value retrieved from the Windows API, normalized against the standard DPI of 96.0f. If the DPI retrieval function is not available or fails, this function returns 1.0f as a default scaling factor.
         */
        float dpi_scale_for_window(HWND hwnd) noexcept
        {
            using get_dpi_for_window_fn = UINT(WINAPI *)(HWND);
            // Dynamically load the GetDpiForWindow function from user32.dll, as it may not be available on older versions of Windows.
            static const auto fn = reinterpret_cast<get_dpi_for_window_fn>(
                GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));

            if (!fn) // If the function is not available, return a default scaling factor of 1.0f (no scaling).
                return 1.0f;

            const UINT dpi = fn(hwnd);
            if (dpi == 0) // If the function fails to retrieve a valid DPI value, return a default scaling factor of 1.0f (no scaling).
                return 1.0f;

            return static_cast<float>(dpi) / 96.0f;
        }

        float dpi_scale_for_system() noexcept
        {
            using get_dpi_for_system_fn = UINT(WINAPI *)();
            static const auto fn = reinterpret_cast<get_dpi_for_system_fn>(
                GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForSystem"));

            if (!fn)
                return 1.0f;

            const UINT dpi = fn();
            if (dpi == 0)
                return 1.0f;

            return static_cast<float>(dpi) / 96.0f;
        }

        void enqueue_event(std::unique_ptr<core::event_base> e)
        {
            if (!e)
                return;

            e->stamp();

            // Exactly one delivery path: if the application installed an event_sink the event is published to it right
            // away and nothing is queued; otherwise it is queued for poll_event(). Doing both would deliver every event
            // twice to applications that use both APIs, and leak the queue in applications that use only the sink.
            if (g_event_sink)
            {
                g_event_sink->publish(*e);
                return;
            }

            g_events.push_back(std::move(e));
        }

        /**
         * @fn enqueue_event
         * @brief Enqueues an event of a specific type by creating a unique pointer to the event and adding it to the global events deque. This function is a convenience overload that allows callers to enqueue an event by value, without needing to manually create a unique pointer. The event is timestamped and then delivered through exactly one path: published to the global event_sink if one is set, otherwise stored in the global events deque for poll_event(). By using perfect forwarding, this function can efficiently handle both lvalue and rvalue events, allowing for optimal performance when enqueuing events of various types.
         * @param e The event to enqueue, passed by value. This can be an lvalue or an rvalue, and the function will handle it appropriately to create a unique pointer for storage and publication.
         * @tparam E The type of the event being enqueued. This must be a type that derives from core::event_base, as it will be stored in the global events deque and published through the event_sink.
         */
        template <typename E>
        void enqueue_event(E e)
        {
            std::unique_ptr<core::event_base> event = std::make_unique<E>(std::move(e));
            enqueue_event(std::move(event));
        }

        /**
         * @fn resolve_px
         * @brief Resolves a ui::length measurement to pixels based on the specified axis and DPI scaling factor. This function takes a ui::length value, which can represent various types of measurements (e.g. absolute pixels, percentages, etc.), and resolves it to an integer pixel value that can be used for rendering and layout. The resolution process takes into account the specified axis (horizontal or vertical) and the DPI scaling factor for the window, allowing for accurate conversion of measurements to pixels in the context of high-DPI displays.
         * @param v The ui::length value to resolve to pixels. This can represent various types of measurements, and the resolution process will convert it to an integer pixel value based on the specified axis and DPI scaling factor.
         * @param a The axis (horizontal or vertical) that the length measurement applies to. This is used to determine how percentage-based measurements should be resolved, as they depend on the size of the parent element along the specified axis.
         * @param dpi_scale The DPI scaling factor for the window, used to convert dp units to pixels. This is typically retrieved using the dpi_scale_for_window function, and it allows for accurate resolution of measurements in the context of high-DPI displays.
         * @return The resolved pixel value as an integer, representing the measurement in pixels based on the specified axis and DPI scaling factor. This value can be used for rendering and layout calculations within the application.
         */
        std::int32_t resolve_px(const ui::length &v, ui::axis a, float dpi_scale) noexcept
        {
            ui::resolve_context ctx{};
            ctx.dpi_scale = dpi_scale;
            ctx.dpi_x = dpi_scale * 96.0f;
            ctx.dpi_y = dpi_scale * 96.0f;
            const float px = ui::resolve_or(v, a, ctx, 0.0f);
            return static_cast<std::int32_t>(px);
        }

        /**
         * @fn to_input_mouse_button
         * @brief Converts a Windows mouse message and its parameters into a corresponding input::mouse_button enum value. This function takes the message code (e.g. WM_LBUTTONDOWN, WM_RBUTTONUP, etc.) and the WPARAM parameter, which may contain additional information for certain messages (e.g. for WM_XBUTTONDOWN/UP), and maps them to the appropriate mouse button representation defined in the input module. If the message does not correspond to a known mouse button event, this function returns input::mouse_button::unknown.
         * @param msg The Windows message code representing the mouse event (e.g. WM_LBUTTONDOWN, WM_RBUTTONUP, etc.). This is used to determine which mouse button event occurred.
         * @param wparam The WPARAM parameter from the Windows message, which may contain additional information for certain messages (e.g. for WM_XBUTTONDOWN/UP). This is used to determine which specific button was involved in the event when processing extended mouse button messages.
         * @return The corresponding input::mouse_button enum value representing the mouse button involved in the event. If the message does not correspond to a known mouse button event, this function returns input::mouse_button::unknown.
         */
        input::mouse_button to_input_mouse_button(UINT msg, WPARAM wparam) noexcept
        {
            switch (msg)
            {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                return input::mouse_button::left;
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
                return input::mouse_button::right;
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                return input::mouse_button::middle;
            // For extended mouse buttons, we need to check the WPARAM to determine which button was involved (XBUTTON1 or XBUTTON2).
            // The GET_XBUTTON_WPARAM macro is used to extract the button information from the hi part of WPARAM for these messages.
            // Windows only supports two extended mouse buttons (XBUTTON1 and XBUTTON2), so we can directly map them to input::mouse_button::x1 and input::mouse_button::x2 respectively.
            // Some mouse drivers may support additional buttons, but Windows will still only report them as XBUTTON1 or XBUTTON2, so we will treat any button that is not XBUTTON1 as XBUTTON2 for the purposes of this mapping.
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
                return (GET_XBUTTON_WPARAM(wparam) == XBUTTON1) ? input::mouse_button::x1 : input::mouse_button::x2;
            default:
                return input::mouse_button::unknown;
            }
        }

        /**
         * @fn current_modifiers
         * @brief Retrieves the current state of key modifiers (e.g. Shift, Control, Alt, etc.) by querying the Windows API for the state of relevant keys. This function checks the state of modifier keys using GetKeyState and constructs an input::key_modifiers value that represents the currently active modifiers. This can be used to determine which modifier keys are currently pressed when processing input events, allowing for accurate handling of keyboard shortcuts and other interactions that depend on modifier keys.
         * @return An input::key_modifiers value representing the currently active key modifiers. This is constructed by checking the state of relevant keys (e.g. Shift, Control, Alt, etc.) using the Windows API and combining their states into a single value that can be used for input processing.
         */
        input::key_modifiers current_modifiers() noexcept
        {
            input::key_modifiers mods = input::key_modifiers::none;

            if ((GetKeyState(VK_SHIFT) & 0x8000) != 0)
                mods |= input::key_modifiers::shift;
            if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
                mods |= input::key_modifiers::control;
            if ((GetKeyState(VK_MENU) & 0x8000) != 0)
                mods |= input::key_modifiers::alt;
            if ((GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0)
                mods |= input::key_modifiers::super;

            if ((GetKeyState(VK_CAPITAL) & 0x0001) != 0)
                mods |= input::key_modifiers::caps_lock;
            if ((GetKeyState(VK_NUMLOCK) & 0x0001) != 0)
                mods |= input::key_modifiers::num_lock;

            return mods;
        }

        /**
         * @fn to_input_key_code
         * @brief Converts a Windows keyboard message and its parameters into a corresponding input::key_code enum value. This function takes the message code (e.g. WM_KEYDOWN, WM_KEYUP, etc.) and the WPARAM and LPARAM parameters, which contain information about the key event, and maps them to the appropriate key code representation defined in the input module. The conversion process involves checking the virtual key code in WPARAM against known key codes and also considering the extended key flag in LPARAM for certain keys (e.g. distinguishing between Enter on the main keyboard vs. Enter on the numeric keypad). If the message does not correspond to a known key event or if the virtual key code is not recognized, this function returns input::key_code::unknown.
         * @param msg The Windows message code representing the keyboard event (e.g. WM_KEYDOWN, WM_KEYUP, etc.). This is used to determine which type of key event occurred.
         * @param wparam The WPARAM parameter from the Windows message, which contains the virtual key code representing the specific key involved in the event. This is used to determine which key was pressed or released.
         * @param lparam The LPARAM parameter from the Windows message, which contains additional information about the key event, such as whether it was an extended key. This is used to distinguish between certain keys that share virtual key codes but have different meanings based on their position (e.g. Enter on the main keyboard vs. Enter on the numeric keypad).
         * @return The corresponding input::key_code enum value representing the key involved in the event. If the message does not correspond to a known key event or if the virtual key code is not recognized, this function returns input::key_code::unknown.
         */
        input::key_code to_input_key_code(UINT msg, WPARAM wparam, LPARAM lparam) noexcept
        {
            (void)msg;

            const bool extended = (lparam & (1 << 24)) != 0;

            // Letters
            if (wparam >= 'A' && wparam <= 'Z')
            {
                return static_cast<input::key_code>(static_cast<std::uint16_t>(input::key_code::a) +
                                                    static_cast<std::uint16_t>(wparam - 'A'));
            }

            // Digits (top row)
            if (wparam >= '0' && wparam <= '9')
            {
                if (wparam == '0')
                    return input::key_code::digit_0;
                return static_cast<input::key_code>(static_cast<std::uint16_t>(input::key_code::digit_1) +
                                                    static_cast<std::uint16_t>(wparam - '1'));
            }

            switch (wparam)
            {
            case VK_ESCAPE:
                return input::key_code::escape;
            case VK_RETURN:
                return extended ? input::key_code::keypad_enter : input::key_code::enter;
            case VK_BACK:
                return input::key_code::backspace;
            case VK_TAB:
                return input::key_code::tab;
            case VK_SPACE:
                return input::key_code::space;

            case VK_OEM_MINUS:
                return input::key_code::minus;
            case VK_OEM_PLUS:
                return input::key_code::equal;
            case VK_OEM_4:
                return input::key_code::left_bracket;
            case VK_OEM_6:
                return input::key_code::right_bracket;
            case VK_OEM_5:
                return input::key_code::backslash;
            case VK_OEM_1:
                return input::key_code::semicolon;
            case VK_OEM_7:
                return input::key_code::apostrophe;
            case VK_OEM_3:
                return input::key_code::grave_accent;
            case VK_OEM_COMMA:
                return input::key_code::comma;
            case VK_OEM_PERIOD:
                return input::key_code::period;
            case VK_OEM_2:
                return input::key_code::slash;
            case VK_OEM_102:
                return input::key_code::non_us_backslash;

            case VK_CAPITAL:
                return input::key_code::caps_lock;

            case VK_SNAPSHOT:
                return input::key_code::print_screen;
            case VK_SCROLL:
                return input::key_code::scroll_lock;
            case VK_PAUSE:
                return input::key_code::pause;

            case VK_INSERT:
                return input::key_code::insert;
            case VK_HOME:
                return input::key_code::home;
            case VK_PRIOR:
                return input::key_code::page_up;
            case VK_DELETE:
                return input::key_code::delete_key;
            case VK_END:
                return input::key_code::end;
            case VK_NEXT:
                return input::key_code::page_down;

            case VK_RIGHT:
                return input::key_code::right_arrow;
            case VK_LEFT:
                return input::key_code::left_arrow;
            case VK_DOWN:
                return input::key_code::down_arrow;
            case VK_UP:
                return input::key_code::up_arrow;

            case VK_NUMLOCK:
                return input::key_code::num_lock;
            case VK_DIVIDE:
                return input::key_code::keypad_divide;
            case VK_MULTIPLY:
                return input::key_code::keypad_multiply;
            case VK_SUBTRACT:
                return input::key_code::keypad_minus;
            case VK_ADD:
                return input::key_code::keypad_plus;
            case VK_DECIMAL:
                return input::key_code::keypad_period;

            case VK_NUMPAD0:
                return input::key_code::keypad_0;
            case VK_NUMPAD1:
                return input::key_code::keypad_1;
            case VK_NUMPAD2:
                return input::key_code::keypad_2;
            case VK_NUMPAD3:
                return input::key_code::keypad_3;
            case VK_NUMPAD4:
                return input::key_code::keypad_4;
            case VK_NUMPAD5:
                return input::key_code::keypad_5;
            case VK_NUMPAD6:
                return input::key_code::keypad_6;
            case VK_NUMPAD7:
                return input::key_code::keypad_7;
            case VK_NUMPAD8:
                return input::key_code::keypad_8;
            case VK_NUMPAD9:
                return input::key_code::keypad_9;

            case VK_F1:
                return input::key_code::f1;
            case VK_F2:
                return input::key_code::f2;
            case VK_F3:
                return input::key_code::f3;
            case VK_F4:
                return input::key_code::f4;
            case VK_F5:
                return input::key_code::f5;
            case VK_F6:
                return input::key_code::f6;
            case VK_F7:
                return input::key_code::f7;
            case VK_F8:
                return input::key_code::f8;
            case VK_F9:
                return input::key_code::f9;
            case VK_F10:
                return input::key_code::f10;
            case VK_F11:
                return input::key_code::f11;
            case VK_F12:
                return input::key_code::f12;
            case VK_F13:
                return input::key_code::f13;
            case VK_F14:
                return input::key_code::f14;
            case VK_F15:
                return input::key_code::f15;
            case VK_F16:
                return input::key_code::f16;
            case VK_F17:
                return input::key_code::f17;
            case VK_F18:
                return input::key_code::f18;
            case VK_F19:
                return input::key_code::f19;
            case VK_F20:
                return input::key_code::f20;
            case VK_F21:
                return input::key_code::f21;
            case VK_F22:
                return input::key_code::f22;
            case VK_F23:
                return input::key_code::f23;
            case VK_F24:
                return input::key_code::f24;

            default:
                return input::key_code::unknown;
            }
        }

        /**
         * @fn utf32_from_utf16_unit
         * @brief Converts a single UTF-16 code unit (wchar_t) from Windows into a UTF-32 code point (input::character_code). This function handles the conversion of UTF-16 code units, including the processing of surrogate pairs for characters outside the Basic Multilingual Plane (BMP). If a high surrogate is encountered, it is stored in a thread-local variable until the corresponding low surrogate is received. If a low surrogate is received without a pending high surrogate, or if an invalid code unit is encountered, this function returns the Unicode replacement character (U+FFFD) to indicate an error in decoding.
         * @param unit The UTF-16 code unit (wchar_t) to convert. This is typically obtained from the WPARAM of a WM_CHAR message in the Windows message loop, which provides UTF-16 encoded character input.
         * @return The corresponding UTF-32 code point as an input::character_code. If the input code unit is part of a valid surrogate pair, the combined code point is returned. If an invalid code unit is encountered (e.g. a low surrogate without a pending high surrogate), the Unicode replacement character (U+FFFD) is returned to indicate an error in decoding. If the code unit is a valid non-surrogate character, it is returned directly as a UTF-32 code point.
         */
        input::character_code utf32_from_utf16_unit(wchar_t unit) noexcept
        {
            // Windows UTF-16. wchar_t is 16-bit on Windows.
            const std::uint32_t u = static_cast<std::uint16_t>(unit);

            if (u >= 0xD800u && u <= 0xDBFFu)
            {
                g_pending_high_surrogate = unit;
                return 0;
            }

            if (u >= 0xDC00u && u <= 0xDFFFu)
            {
                if (g_pending_high_surrogate == 0)
                    return U'\uFFFD';

                const std::uint32_t hi = static_cast<std::uint16_t>(g_pending_high_surrogate);
                g_pending_high_surrogate = 0;
                const std::uint32_t lo = u;
                const std::uint32_t cp = 0x10000u + (((hi - 0xD800u) << 10) | (lo - 0xDC00u));
                return static_cast<input::character_code>(cp);
            }

            g_pending_high_surrogate = 0;
            return static_cast<input::character_code>(u);
        }

        /**
         * @fn window_proc
         * @brief The window procedure function that processes messages sent to windows created by this platform implementation. This function is called by the Windows API whenever a message is sent to a window, and it is responsible for handling various messages related to window events (e.g. close, resize, DPI changes) and input events (e.g. keyboard and mouse events). The function retrieves the window ID associated with the HWND from the GWLP_USERDATA, and then processes the message accordingly, generating appropriate events and enqueuing them for processing through the event_sink. The function returns an LRESULT value that indicates how the message was handled, allowing for proper integration with the Windows message loop.
         * @param hwnd The handle to the window that received the message. This is used to identify which window the message is associated with and to retrieve the corresponding window ID for event generation.
         * @param msg The Windows message code representing the event or action that occurred. This is used to determine how to process the message and which events to generate in response.
         * @param wparam The WPARAM parameter from the Windows message, which may contain additional information relevant to the message being processed (e.g. virtual key codes for keyboard events, button information for mouse events, etc.).
         * @param lparam The LPARAM parameter from the Windows message, which may contain additional information relevant to the message being processed (e.g. additional flags for keyboard events, mouse position information for mouse events, etc.).
         * @return An LRESULT value indicating how the message was handled. This typically returns 0 if the message was handled and should not be processed further by the default window procedure, or it may return a non-zero value if the message was not handled and should be processed by the default window procedure. The specific return value may depend on the message being processed and how it was handled within this function.
         * @note This function is registered as the window procedure for windows created by this implementation, and it is responsible for translating Windows messages into the appropriate events defined in the catalyst platform and input modules, allowing for consistent event handling across the application. By processing messages in this function, we can ensure that events are generated in response to user interactions and system events, enabling the application to respond accordingly.
         */
        LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
        {
            const auto id = static_cast<window_id>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

            switch (msg)
            {
            case WM_CLOSE:
            {
                if (id != 0)
                {
                    window_close_requested_event we;
                    we.window = id;
                    enqueue_event(we);
                }
                return 0; // app decides when to destroy
            }
            case WM_DESTROY:
            {
                if (id != 0)
                {
                    window_destroyed_event we;
                    we.window = id;
                    enqueue_event(we);
                }
                return 0;
            }
            case WM_SIZE:
            {
                if (id != 0)
                {
                    window_resized_event we;
                    we.window = id;
                    we.width_px = ui::px(LOWORD(lparam));
                    we.height_px = ui::px(HIWORD(lparam));
                    enqueue_event(we);
                }
                break;
            }
            case WM_ENTERSIZEMOVE:
            {
                if (id != 0)
                {
                    window_enter_size_move_event we;
                    we.window = id;
                    enqueue_event(we);
                }
                break;
            }
            case WM_EXITSIZEMOVE:
            {
                if (id != 0)
                {
                    window_exit_size_move_event we;
                    we.window = id;
                    enqueue_event(we);
                }
                break;
            }
            case WM_DPICHANGED:
            {
                if (id != 0)
                {
                    // Resize to suggested rect for new DPI.
                    const RECT *suggested = reinterpret_cast<const RECT *>(lparam);
                    SetWindowPos(hwnd, nullptr,
                                 suggested->left,
                                 suggested->top,
                                 suggested->right - suggested->left,
                                 suggested->bottom - suggested->top,
                                 SWP_NOZORDER | SWP_NOACTIVATE);

                    window_dpi_changed_event we;
                    we.window = id;
                    we.dpi_scale = dpi_scale_for_window(hwnd);
                    enqueue_event(we);
                }
                return 0;
            }

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            {
                if (id != 0)
                {
                    const bool repeat = (lparam & (1 << 30)) != 0;

                    input::key_event ke;
                    ke.code = to_input_key_code(msg, wparam, lparam);
                    ke.native_code = input::to_usb_hid(ke.code);
                    ke.action = repeat ? input::key_action::repeat : input::key_action::press;
                    ke.modifiers = current_modifiers();
                    enqueue_event(ke);
                }
                break;
            }
            case WM_KEYUP:
            case WM_SYSKEYUP:
            {
                if (id != 0)
                {
                    input::key_event ke;
                    ke.code = to_input_key_code(msg, wparam, lparam);
                    ke.native_code = input::to_usb_hid(ke.code);
                    ke.action = input::key_action::release;
                    ke.modifiers = current_modifiers();
                    enqueue_event(ke);
                }
                break;
            }

            case WM_CHAR:
            {
                if (id != 0)
                {
                    const auto cp = utf32_from_utf16_unit(static_cast<wchar_t>(wparam));
                    if (cp != 0)
                    {
                        input::character_event ce;
                        ce.character = cp;
                        ce.modifiers = current_modifiers();
                        enqueue_event(ce);

                        const char32_t one[1] = {static_cast<char32_t>(cp)};
                        input::text_input_event te{std::span<const char32_t>(one, 1)};
                        enqueue_event(te);
                    }
                }
                break;
            }
            case WM_MOUSEMOVE:
            {
                if (id != 0)
                {
                    const auto x = static_cast<std::int32_t>(GET_X_LPARAM(lparam));
                    const auto y = static_cast<std::int32_t>(GET_Y_LPARAM(lparam));

                    const math::vec2<std::int32_t> pos{x, y};
                    const auto it = g_last_mouse_pos_px.find(id);
                    const math::vec2<std::int32_t> last = (it != g_last_mouse_pos_px.end()) ? it->second : pos;
                    g_last_mouse_pos_px[id] = pos;

                    input::mouse_move_event me;
                    me.position_px = pos;
                    me.delta_px = {pos.x - last.x, pos.y - last.y};
                    enqueue_event(me);
                }
                break;
            }
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_XBUTTONDOWN:
            {
                if (id != 0)
                {
                    SetCapture(hwnd);

                    input::mouse_button_event be;
                    be.button = to_input_mouse_button(msg, wparam);
                    be.action = input::mouse_button_action::press;
                    be.position_px = {static_cast<std::int32_t>(GET_X_LPARAM(lparam)), static_cast<std::int32_t>(GET_Y_LPARAM(lparam))};
                    enqueue_event(be);
                }
                if (msg == WM_XBUTTONDOWN)
                    return TRUE;
                break;
            }
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
            case WM_XBUTTONUP:
            {
                if (id != 0)
                {
                    ReleaseCapture();

                    input::mouse_button_event be;
                    be.button = to_input_mouse_button(msg, wparam);
                    be.action = input::mouse_button_action::release;
                    be.position_px = {static_cast<std::int32_t>(GET_X_LPARAM(lparam)), static_cast<std::int32_t>(GET_Y_LPARAM(lparam))};
                    enqueue_event(be);
                }
                if (msg == WM_XBUTTONUP)
                    return TRUE;
                break;
            }
            case WM_MOUSEWHEEL:
            {
                if (id != 0)
                {
                    const float delta_y = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / 120.0f;

                    POINT p{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)}; // screen coords
                    ScreenToClient(hwnd, &p);

                    input::mouse_wheel_event we;
                    we.position_px = {static_cast<std::int32_t>(p.x), static_cast<std::int32_t>(p.y)};
                    we.delta = {0.0f, delta_y};
                    enqueue_event(we);
                }
                break;
            }
            default:
                break;
            }

            return DefWindowProcW(hwnd, msg, wparam, lparam);
        }

        /**
         * @fn ensure_window_class_registered
         * @brief Ensures that the window class used for creating windows in this platform implementation is registered with the Windows API.
         * @details This function checks if the window class has already been registered, and if not, it registers the class by filling out a WNDCLASSEXW structure with the appropriate parameters (e.g. window procedure, instance handle, class name, cursor) and calling RegisterClassExW. By using a static local variable to track whether the class has been registered, this function ensures that the registration process only occurs once, even if multiple windows are created using this implementation. Windows classes are a fundamental part of the Windows API for creating and managing windows, and registering a class is necessary before any windows can be created using that class. By centralizing the registration logic in this function, we can ensure that all windows created by this platform implementation use the same registered class, which simplifies the window creation process and ensures consistency across all windows.
         * @note This function must be called before creating any windows using this implementation, as the window class must be registered with the Windows API in order to create windows successfully. By centralizing the registration logic in this function, we can ensure that the necessary setup is performed consistently and efficiently across all windows created by this platform implementation.
         */
        void ensure_window_class_registered()
        {
            static bool registered = false;
            if (registered)
                return;

            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = &window_proc;                              // The window procedure function that will process messages for windows created with this class. This is set to the window_proc function defined in this implementation, which handles various messages related to window events and input events, allowing for consistent event handling across the application.
            wc.hInstance = GetModuleHandleW(nullptr);                   // The handle to the instance of the module that contains the window procedure. This is typically obtained using GetModuleHandleW with a null parameter to get the handle for the current module. This handle is used by the Windows API to identify which module is responsible for handling messages for windows created with this class, and it is necessary for proper message routing and handling within the application.
            wc.lpszClassName = k_window_class_name;                     // The name of the window class being registered. This is used to identify the class when creating windows, and it must be unique within the application. By defining this as a constant, we can ensure that all windows created by this implementation use the same class name, which simplifies the window creation process and ensures consistency across all windows.
            wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // Load the default arrow cursor for windows created with this class. This is done using LoadCursorW with a null module handle and the MAKEINTRESOURCEW macro to specify the standard IDC_ARROW cursor. By setting this cursor for the window class, we ensure that all windows created with this class will use the default arrow cursor, providing a consistent user experience across the application.

            RegisterClassExW(&wc);
            registered = true;
        }

        /**
         * @fn hwnd_from_id
         * @brief Retrieves the HWND (window handle) associated with a given window_id. This function looks up the window_id in the g_windows map, which stores the mapping between window IDs and their corresponding HWNDs. If the window_id is found in the map, the associated HWND is returned; otherwise, nullptr is returned to indicate that the window_id is not valid or does not correspond to an existing window. This function is used internally to retrieve the HWND for a given window_id when processing events or performing operations on windows, allowing for efficient mapping between our internal window identifiers and the native Windows handles.
         * @param id The window_id for which to retrieve the corresponding HWND. This is an internal identifier used by our platform implementation to represent windows, and it is mapped to the native HWND through the g_windows map. By providing a window_id, we can look up the associated HWND and perform operations or generate events for that window as needed.
         * @return The HWND associated with the given window_id if it exists; otherwise, nullptr if the window_id is not valid or does not correspond to an existing window. This allows the function to indicate whether the provided window_id is valid and can be used for further operations, or if it should be treated as invalid (e.g. when processing events or performing actions on windows). By returning nullptr for invalid window_ids, we can ensure that our platform implementation handles such cases gracefully and avoids potential errors when working with windows.
         */
        HWND hwnd_from_id(window_id id) noexcept
        {
            auto it = g_windows.find(id);
            if (it == g_windows.end())
                return nullptr;
            return it->second;
        }
    }

    /**
     * @fn create_window
     * @brief Creates a new window based on the provided window_desc structure, which contains parameters such as title, dimensions, and whether the window should be resizable or initially visible. This function first ensures that the window class is registered with the Windows API, then it generates a unique window_id for the new window and constructs the appropriate window styles based on the provided description. It converts the title from UTF-8 to a wide string for use with Windows APIs, and calculates the required size of the window based on the desired client area dimensions and the system DPI scaling. The CreateWindowExW function is then called to create the window, and if successful, the new window's HWND is stored in the g_windows map with its associated window_id. If the window is created successfully and is marked as initially visible, it is shown using ShowWindow. Finally, initial events for window resizing and DPI changes are generated and enqueued to ensure that the application has accurate information about the new window's state from the moment it is created.
     * @param desc A window_desc structure containing the parameters for the window to be created, including title, width, height, resizable flag, and initial visibility. This structure allows the caller to specify the desired characteristics of the window being created, and the function will use this information to configure the window accordingly. The title is used for the window's title bar, the width and height specify the desired client area dimensions, the resizable flag determines whether the window can be resized by the user, and the initial visibility flag indicates whether the window should be shown immediately after creation. By providing a comprehensive description of the window to be created, this function can create windows that meet the specific needs of the application while ensuring proper integration with the Windows API and consistent event generation for the application's event handling system.
     * @return A window_id representing the newly created window if the creation is successful; otherwise, 0 if the window could not be created. This allows the caller to check whether the window was created successfully and to obtain a valid identifier for the new window that can be used for further operations (e.g. event handling, window management, etc.). By returning 0 for failed creations, we can ensure that the caller can handle such cases gracefully and avoid potential errors when working with windows that were not created successfully.
     */
    window_id create_window(const window_desc &desc)
    {
        ensure_window_class_registered();

        // Get next window id and reserve it before creating the window to avoid potential race conditions with the window procedure.
        const window_id id = g_next_window_id++;

        // Construct window styles based on the provided description. If the window is marked as resizable, we use the WS_OVERLAPPEDWINDOW style, which includes a title bar, border, and resizing capabilities. If the window is not resizable, we use a combination of WS_OVERLAPPED, WS_CAPTION, WS_SYSMENU, and WS_MINIMIZEBOX styles to create a fixed-size window with a title bar and system menu but without resizing capabilities. The extended style is set to WS_EX_APPWINDOW to ensure that the window appears in the taskbar and has appropriate behavior as a top-level application window.
        const DWORD style = desc.resizable ? WS_OVERLAPPEDWINDOW : (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);
        const DWORD ex_style = WS_EX_APPWINDOW;

        // Convert the title from UTF-8 to a wide string for use with Windows APIs. If the title is null, we use a default title of "Catalyst". If the conversion results in an empty string (e.g. if the input is not valid UTF-8), we also fall back to the default title. This ensures that we always have a valid title for the window, even if the input is not properly encoded or if no title is provided.
        std::wstring titleW = desc.title ? win32::utf8_to_wide_or_ansi(desc.title) : L"Catalyst";
        if (titleW.empty())
            titleW = L"Catalyst";

        // Calculate the required size of the window based on the desired client area dimensions and the system DPI scaling. The resolve_px function is used to convert the width and height from the window_desc, which may be specified in different units (e.g. pixels, points, etc.), into actual pixel dimensions based on the current system DPI scaling. This ensures that the window is created with the correct size on high-DPI displays, providing a consistent user experience across different screen resolutions and DPI settings.
        const float system_scale = dpi_scale_for_system();
        const std::int32_t width_px = resolve_px(desc.width_px, ui::axis::x, system_scale);
        const std::int32_t height_px = resolve_px(desc.height_px, ui::axis::y, system_scale);

        // Calculate the required size of the window based on the desired client area dimensions and the window styles. The AdjustWindowRectEx function is used to adjust the size of the window rectangle to account for the non-client area (e.g. title bar, borders) based on the specified styles. This ensures that when we create the window with the calculated size, the resulting client area will match the desired dimensions specified in the window_desc, providing a consistent layout and appearance for the application.
        RECT r{0, 0, static_cast<LONG>(width_px), static_cast<LONG>(height_px)};
        AdjustWindowRectEx(&r, style, FALSE, ex_style);

        HWND hwnd = CreateWindowExW(
            ex_style,
            k_window_class_name,
            titleW.c_str(),
            style,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            r.right - r.left,
            r.bottom - r.top,
            nullptr,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        if (!hwnd)
            return 0;

        SetWindowLongPtrW(hwnd, GWLP_USERDATA, static_cast<LONG_PTR>(id));
        g_windows.emplace(id, hwnd);

        if (desc.visible)
            ShowWindow(hwnd, SW_SHOW);

        // Initial events
        {
            window_resized_event e;
            e.window = id;
            RECT cr{};
            GetClientRect(hwnd, &cr);
            e.width_px = ui::px(static_cast<std::int32_t>(cr.right - cr.left));
            e.height_px = ui::px(static_cast<std::int32_t>(cr.bottom - cr.top));
            enqueue_event(e);
        }
        {
            window_dpi_changed_event e;
            e.window = id;
            e.dpi_scale = dpi_scale_for_window(hwnd);
            enqueue_event(e);
        }

        return id;
    }

    /**
     * @fn destroy_window
     * @brief Destroys the window associated with the given window_id. This function first retrieves the HWND associated with the provided window_id using the hwnd_from_id function. If the HWND is valid, it calls DestroyWindow to destroy the window, which will trigger the appropriate messages (e.g. WM_DESTROY) that are handled in the window procedure to generate events for the application. After calling DestroyWindow, the function removes the entry for the window_id from the g_windows map to clean up the internal state and ensure that the window_id is no longer considered valid for future operations. By destroying the window through this function, we allow the application to properly handle window destruction events and perform any necessary cleanup or state updates in response to the window being closed.
     * @param id The window_id of the window to be destroyed. This is an internal identifier used by our platform implementation to represent windows, and it is mapped to the native HWND through the g_windows map. By providing a window_id, we can look up the associated HWND and call DestroyWindow to initiate the destruction process for that window. After destroying the window, we also remove the entry for the window_id from the g_windows map to ensure that it is no longer considered valid for future operations.
     */
    void destroy_window(window_id id) noexcept
    {
        HWND hwnd = hwnd_from_id(id);
        if (!hwnd)
            return;

        DestroyWindow(hwnd);
        g_windows.erase(id);
    }

    /**
     * @fn is_window_valid
     * @brief Checks whether the window associated with the given window_id is valid and exists. This function retrieves the HWND associated with the provided window_id using the hwnd_from_id function, and then it checks if the HWND is not null and if it corresponds to an existing window using the IsWindow function from the Windows API. If both conditions are true, the function returns true, indicating that the window is valid; otherwise, it returns false. This allows the application to check whether a given window_id corresponds to a valid and existing window before performing operations on it or generating events related to that window.
     * @param id The window_id of the window to check for validity. This is an internal identifier used by our platform implementation to represent windows, and it is mapped to the native HWND through the g_windows map. By providing a window_id, we can look up the associated HWND and check if it is valid and corresponds to an existing window using the Windows API.
     * @return true if the window associated with the given window_id is valid and exists; otherwise, false if the window_id does not correspond to a valid or existing window. This allows the application to handle cases where a window may have been destroyed or where an invalid window_id is provided, ensuring that operations are only performed on valid windows and that events are generated appropriately based on the existence of the window.
     */
    bool is_window_valid(window_id id) noexcept
    {
        HWND hwnd = hwnd_from_id(id);
        return hwnd != nullptr && IsWindow(hwnd);
    }

    /**
     * @fn get_native_handle
     * @brief Retrieves the native handle associated with the window identified by the given window_id. This function first retrieves the HWND associated with the provided window_id using the hwnd_from_id function. If the HWND is valid, it constructs a native_handle structure with the kind set to win32_hwnd, the handle set to the retrieved HWND, and the extra field set to the module handle of the current process (obtained using GetModuleHandleW). This allows the application to obtain a native handle that can be used for interoperability with other APIs or libraries that require access to the underlying Windows handle for a window. If the HWND is not valid, an empty native_handle structure is returned, indicating that there is no valid native handle for the given window_id.
     * @param id The window_id of the window for which to retrieve the native handle. This is an internal identifier used by our platform implementation to represent windows, and it is mapped to the native HWND through the g_windows map. By providing a window_id, we can look up the associated HWND and construct a native_handle structure that contains this information for use in interoperability scenarios.
     * @return A native_handle structure containing the kind (win32_hwnd), handle (the retrieved HWND), and extra (the module handle) if the HWND is valid; otherwise, an empty native_handle structure if there is no valid native handle for the given window_id. This allows the application to check whether a valid native handle was obtained and to use it for further operations or interoperability as needed.
     */
    native_handle get_native_handle(window_id id) noexcept
    {
        HWND hwnd = hwnd_from_id(id);
        if (!hwnd)
            return {};

        native_handle h;
        h.kind = native_handle_kind::win32_hwnd;
        h.handle = hwnd;
        h.extra = GetModuleHandleW(nullptr);
        return h;
    }

    /**
     * @fn client_rect_px
     * @brief Retrieves the client area rectangle of the window identified by the given window_id in pixel coordinates. This function first retrieves the HWND associated with the provided window_id using the hwnd_from_id function. If the HWND is valid, it calls GetClientRect to obtain the dimensions of the client area of the window, which is the area available for rendering and user interaction (excluding title bars, borders, etc.). The resulting RECT structure is then converted into a math::rect<std::int32_t> structure, which contains the top-left and bottom-right coordinates of the client area in pixels. If the HWND is not valid, a default rectangle with zero dimensions is returned, indicating that there is no valid client area for the given window_id.
     * @param id The window_id of the window for which to retrieve the client area rectangle. This is an internal identifier used by our platform implementation to represent windows, and it is mapped to the native HWND through the g_windows map. By providing a window_id, we can look up the associated HWND and call GetClientRect to obtain the dimensions of the client area for that window.
     * @return A math::rect<std::int32_t> structure containing the top-left and bottom-right coordinates of the client area in pixels if the HWND is valid; otherwise, a default rectangle with zero dimensions if there is no valid client area for the given window_id. This allows the application to check whether a valid client area was obtained and to use this information for rendering or layout purposes as needed.
     */
    math::rect<std::int32_t> client_rect_px(window_id id) noexcept
    {
        HWND hwnd = hwnd_from_id(id);
        if (!hwnd)
            return {{0, 0}, {0, 0}};

        RECT r{};
        GetClientRect(hwnd, &r);
        return {{static_cast<std::int32_t>(r.left), static_cast<std::int32_t>(r.top)},
                {static_cast<std::int32_t>(r.right), static_cast<std::int32_t>(r.bottom)}};
    }

    /**
     * @fn dpi_scale
     * @brief Retrieves the DPI scaling factor for the window identified by the given window_id. This function first retrieves the HWND associated with the provided window_id using the hwnd_from_id function. If the HWND is valid, it calls the dpi_scale_for_window function to obtain the current DPI scaling factor for that specific window, which is used to scale UI elements and ensure proper sizing on high-DPI displays. If the HWND is not valid, a default DPI scaling factor of 1.0f is returned, indicating that there is no valid window for which to retrieve DPI information.
     * @param id The window_id of the window for which to retrieve the DPI scaling factor. This is an internal identifier used by our platform implementation to represent windows, and it is mapped to the native HWND through the g_windows map. By providing a window_id, we can look up the associated HWND and call dpi_scale_for_window to obtain the DPI scaling factor for that window.
     * @return The DPI scaling factor for the specified window if the HWND is valid; otherwise, a default value of 1.0f if there is no valid window for which to retrieve DPI information. This allows the application to check whether a valid DPI scaling factor was obtained and to use this information for rendering or layout purposes as needed.
     */
    float dpi_scale(window_id id) noexcept
    {
        HWND hwnd = hwnd_from_id(id);
        if (!hwnd)
            return 1.0f;

        return dpi_scale_for_window(hwnd);
    }

    /**
     * @fn pump_events
     * @brief Pumps and processes all pending Windows messages for the application. This function uses a loop to call PeekMessageW with the PM_REMOVE flag, which retrieves messages from the message queue without blocking and removes them from the queue. For each retrieved message, it calls TranslateMessage to perform keyboard message translations (e.g. converting WM_KEYDOWN to WM_CHAR), and then calls DispatchMessageW to dispatch the message to the appropriate window procedure for handling. This function should be called regularly (e.g. once per frame) to ensure that all pending messages are processed and that the application remains responsive to user input and system events.
     */
    void pump_events() noexcept
    {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    /**
     * @fn wait_events
     * @brief Waits for events or messages to be available for processing, with an optional timeout. This function uses the MsgWaitForMultipleObjectsEx function to wait for any kind of input or message to become available without consuming it. The timeout_ms parameter specifies the maximum time to wait in milliseconds, where a value of 0xFFFFFFFFu indicates an infinite timeout. If the function returns WAIT_TIMEOUT, it means that the specified timeout elapsed without any events becoming available, and the function returns false. If the function returns WAIT_FAILED, it indicates an error occurred while waiting for events, and the function also returns false. If the function returns a value indicating that events are available, it returns true, allowing the application to proceed with processing those events. This function can be used to efficiently wait for events without busy-waiting, improving performance and responsiveness in scenarios where the application may be idle until user input or system events occur.
     * @param timeout_ms The maximum time to wait for events in milliseconds, where 0xFFFFFFFFu indicates an infinite timeout. This allows the caller to specify how long to wait for events before timing out, providing flexibility in how the application handles idle periods and responsiveness to user input or system events.
     * @return true if events are available for processing; false if the specified timeout elapsed without any events becoming available or if an error occurred while waiting for events. This allows the caller to check whether it can proceed with event processing or if it should handle a timeout or error condition accordingly.
     */
    bool wait_events(std::uint32_t timeout_ms) noexcept
    {
        const DWORD timeout = (timeout_ms == 0xFFFFFFFFu) ? INFINITE : static_cast<DWORD>(timeout_ms);

        // Wake for any kind of input/message without consuming it.
        const DWORD rc = MsgWaitForMultipleObjectsEx(
            0,
            nullptr,
            timeout,
            QS_ALLINPUT,
            MWMO_INPUTAVAILABLE);

        if (rc == WAIT_TIMEOUT)
            return false;
        else if (rc == WAIT_FAILED)
            return false;
        else
            return true;
    }

    /**
     * @fn poll_event
     * @brief Polls for the next available event from the internal event queue. This function checks if there are any events in the g_events deque, and if so, it retrieves the front event, moves it into the provided output parameter (out), and removes it from the queue. If an event was successfully retrieved, the function returns true; otherwise, it returns false to indicate that there are no events available for polling. This function allows the application to process events one at a time in a non-blocking manner, enabling efficient event handling while maintaining responsiveness.
     * @param out A reference to a std::unique_ptr<core::event_base> where the next available event will be stored if one is present in the queue. The function will move the front event from the g_events deque into this output parameter if an event is available, allowing the caller to access and process the event after polling.
     * @return true if an event was successfully retrieved and stored in the output parameter; false if there are no events available in the queue for polling. This allows the caller to check whether it can proceed with processing an event or if it should wait for more events to become available.
     */
    bool poll_event(std::unique_ptr<core::event_base> &out) noexcept
    {
        if (g_events.empty())
            return false;

        out = std::move(g_events.front());
        g_events.pop_front();
        return true;
    }

    /**
     * @fn set_event_sink
     * @brief Sets the event sink that will receive events generated by this platform implementation. The event sink is a pointer to an object that implements the core::event_sink interface, which defines a method for receiving events. By setting the event sink, we allow the application to specify where events generated by this platform implementation should be sent for processing. This function simply assigns the provided event sink pointer to the global g_event_sink variable, which is used internally to enqueue events when they are generated in response to Windows messages or other actions. By providing this function, we enable flexibility in how events are handled and processed by allowing the application to define its own event sink implementation.
     * @param sink A pointer to an object that implements the core::event_sink interface, which will receive events generated by this platform implementation. This allows the application to specify where events should be sent for processing, enabling custom handling of events based on the application's needs.
     */
    void set_event_sink(core::event_sink *sink) noexcept
    {
        g_event_sink = sink;
    }

} // namespace catalyst::platform::detail
