#include <catalyst/platform/window.hpp>

#include <catalyst/core/event_sink.hpp>
#include <catalyst/input/keyboard.hpp>
#include <catalyst/input/mouse.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // For basic Windows API functions and types (e.g. HWND, HMONITOR, GetKeyState, etc.).

#include "win32_helpers.hpp"

#include <bitset>
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
        /**
         * @struct window_state
         * @brief Everything the backend keeps per window besides the HWND: the input bookkeeping needed to derive mouse
         * deltas, enter/leave transitions, surrogate-pair decoding and the cursor mode.
         */
        struct window_state
        {
            HWND hwnd = nullptr;
            math::vec2<std::int32_t> last_mouse_pos_px{};
            bool has_mouse_pos = false;   ///< false until the first WM_MOUSEMOVE, so the first delta is zero
            bool mouse_inside = false;    ///< the cursor is over the client area and a WM_MOUSELEAVE is pending
            cursor_mode cursor = cursor_mode::normal;
            wchar_t pending_high_surrogate = 0;
        };

        std::unordered_map<window_id, window_state> g_windows;
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
         * @var g_keys_down
         * @brief USB HID usage ids of the keys the backend currently considers held. Used to publish releases when the
         * focused window loses focus, so applications never see a key stuck down after Alt+Tab.
         */
        std::bitset<input::key_code_count> g_keys_down;
        /**
         * @var g_mouse_buttons_down
         * @brief The mouse buttons currently held, mirrored from the button messages. The mouse is captured while this is
         * non-empty so drags keep reporting after the cursor leaves the window.
         */
        input::mouse_buttons g_mouse_buttons_down = input::mouse_buttons::none;
        /**
         * @var g_mouse_capture_window
         * @brief The window that currently holds the mouse capture on our behalf, or 0.
         */
        window_id g_mouse_capture_window = 0;
        /**
         * @var g_raw_mouse_window
         * @brief The window raw mouse input is currently registered for (the one whose cursor is captured), or 0.
         */
        window_id g_raw_mouse_window = 0;
        /**
         * @var g_last_raw_absolute
         * @brief Last absolute raw-mouse position, for devices (RDP, tablets) that report absolute rather than relative motion.
         */
        math::vec2<std::int32_t> g_last_raw_absolute{};
        bool g_has_last_raw_absolute = false;

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
         * @fn window_state_from_id
         * @brief Looks up the per-window state for an id, or nullptr if the id is not one of ours.
         */
        window_state *window_state_from_id(window_id id) noexcept
        {
            auto it = g_windows.find(id);
            if (it == g_windows.end())
                return nullptr;
            return &it->second;
        }

        /**
         * @fn to_input_mouse_button
         * @brief Maps a Win32 mouse-button message (down, up or double-click) to the input::mouse_button it concerns.
         */
        input::mouse_button to_input_mouse_button(UINT msg, WPARAM wparam) noexcept
        {
            switch (msg)
            {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
                return input::mouse_button::left;
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
                return input::mouse_button::right;
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MBUTTONDBLCLK:
                return input::mouse_button::middle;
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
            case WM_XBUTTONDBLCLK:
                // Windows only has two extended buttons, XBUTTON1 and XBUTTON2.
                return (GET_XBUTTON_WPARAM(wparam) == XBUTTON1) ? input::mouse_button::x1 : input::mouse_button::x2;
            default:
                return input::mouse_button::unknown;
            }
        }

        /**
         * @fn buttons_from_wparam
         * @brief The set of buttons held according to the MK_* flags a mouse message carries in its WPARAM.
         */
        input::mouse_buttons buttons_from_wparam(WPARAM wparam) noexcept
        {
            input::mouse_buttons b = input::mouse_buttons::none;
            if (wparam & MK_LBUTTON)
                b |= input::mouse_buttons::left;
            if (wparam & MK_RBUTTON)
                b |= input::mouse_buttons::right;
            if (wparam & MK_MBUTTON)
                b |= input::mouse_buttons::middle;
            if (wparam & MK_XBUTTON1)
                b |= input::mouse_buttons::x1;
            if (wparam & MK_XBUTTON2)
                b |= input::mouse_buttons::x2;
            return b;
        }

        /**
         * @fn current_modifiers
         * @brief Samples the modifier keys and lock states with GetKeyState, which is synchronised with the message being
         * processed, so the result reflects the state at the time of the event rather than "now".
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
         * @var k_scancode_to_hid
         * @brief Scan code set 1 (what Windows reports in bits 16-23 of a key message's LPARAM) to USB HID keyboard usage,
         * for keys *without* the extended (0xE0) prefix. Indexed by scan code; 0 means "no mapping". Scan codes describe
         * the physical key regardless of the active keyboard layout, which is exactly what input::key_code represents.
         */
        constexpr std::uint8_t k_scancode_to_hid[128] = {
            /* 0x00 */ 0, 41, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 45, 46, 42, 43,
            /* 0x10 */ 20, 26, 8, 21, 23, 28, 24, 12, 18, 19, 47, 48, 40, 224, 4, 22,
            /* 0x20 */ 7, 9, 10, 11, 13, 14, 15, 51, 52, 53, 225, 49, 29, 27, 6, 25,
            /* 0x30 */ 5, 17, 16, 54, 55, 56, 229, 85, 226, 44, 57, 58, 59, 60, 61, 62,
            /* 0x40 */ 63, 64, 65, 66, 67, 83, 71, 95, 96, 97, 86, 92, 93, 94, 87, 89,
            /* 0x50 */ 90, 91, 98, 99, 70, 0, 100, 68, 69, 103, 0, 0, 140, 0, 0, 0,
            /* 0x60 */ 0, 0, 0, 0, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 0,
            /* 0x70 */ 136, 145, 144, 135, 0, 0, 115, 147, 146, 138, 0, 139, 0, 137, 0, 0,
        };

        /**
         * @fn to_input_key_code
         * @brief Translates the scan code and virtual key of a WM_(SYS)KEY* message into the physical key it belongs to.
         * @param wparam The virtual key code. Only consulted for keys whose scan codes collide (Pause/NumLock) or when the
         * message carries no scan code at all (input injected with SendInput), in which case the scan code is recovered
         * with MapVirtualKey.
         * @param lparam The message's LPARAM: bits 16-23 are the scan code, bit 24 the extended-key flag.
         * @param scancode_out Receives the raw scan code, with 0xE000 added for extended keys, for key_event::scancode.
         * @return The key, or input::key_code::unknown for keys Catalyst has no name for (browser/media keys and the
         * phantom Shift messages Windows synthesises around keypad keys).
         */
        input::key_code to_input_key_code(WPARAM wparam, LPARAM lparam, std::uint32_t &scancode_out) noexcept
        {
            using input::key_code;

            const UINT vk = static_cast<UINT>(wparam);
            UINT sc = static_cast<UINT>((lparam >> 16) & 0xFFu);
            bool extended = (lparam & (1 << 24)) != 0;

            if (sc == 0)
            {
                const UINT mapped = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC_EX);
                sc = mapped & 0xFFu;
                extended = (mapped & 0xFF00u) != 0;
            }

            scancode_out = extended ? (0xE000u | sc) : sc;

            // Pause and NumLock share scan code 0x45 (Pause's 0xE1 prefix is dropped by Windows) and PrintScreen shows up
            // with several different codes, so those three are resolved from the virtual key.
            switch (vk)
            {
            case VK_PAUSE:
            case VK_CANCEL:
                return key_code::pause;
            case VK_NUMLOCK:
                return key_code::num_lock;
            case VK_SNAPSHOT:
                return key_code::print_screen;
            default:
                break;
            }

            if (extended)
            {
                switch (sc)
                {
                case 0x1C: return key_code::keypad_enter;
                case 0x1D: return key_code::right_control;
                case 0x20: return key_code::mute;
                case 0x2E: return key_code::volume_down;
                case 0x30: return key_code::volume_up;
                case 0x35: return key_code::keypad_divide;
                case 0x37: return key_code::print_screen;
                case 0x38: return key_code::right_alt;
                case 0x47: return key_code::home;
                case 0x48: return key_code::up_arrow;
                case 0x49: return key_code::page_up;
                case 0x4B: return key_code::left_arrow;
                case 0x4D: return key_code::right_arrow;
                case 0x4F: return key_code::end;
                case 0x50: return key_code::down_arrow;
                case 0x51: return key_code::page_down;
                case 0x52: return key_code::insert;
                case 0x53: return key_code::delete_key;
                case 0x5B: return key_code::left_super;
                case 0x5C: return key_code::right_super;
                case 0x5D: return key_code::application;
                case 0x5E: return key_code::power;
                default:
                    return key_code::unknown; // includes the fake 0xE02A / 0xE036 Shift around keypad keys
                }
            }

            if (sc < 128 && k_scancode_to_hid[sc] != 0)
                return static_cast<key_code>(k_scancode_to_hid[sc]);

            return key_code::unknown;
        }

        /**
         * @fn utf32_from_utf16_unit
         * @brief Feeds one UTF-16 code unit from WM_CHAR into the window's decoder. Returns a complete code point, or 0 when
         * the unit was a high surrogate (the code point is produced when the low surrogate arrives) or invalid.
         */
        input::character_code utf32_from_utf16_unit(window_state &ws, wchar_t unit) noexcept
        {
            const std::uint32_t u = static_cast<std::uint16_t>(unit);

            if (u >= 0xD800u && u <= 0xDBFFu)
            {
                ws.pending_high_surrogate = unit;
                return 0;
            }

            if (u >= 0xDC00u && u <= 0xDFFFu)
            {
                if (ws.pending_high_surrogate == 0)
                    return 0; // stray low surrogate

                const std::uint32_t hi = static_cast<std::uint16_t>(ws.pending_high_surrogate);
                ws.pending_high_surrogate = 0;
                return static_cast<input::character_code>(0x10000u + (((hi - 0xD800u) << 10) | (u - 0xDC00u)));
            }

            ws.pending_high_surrogate = 0;
            return static_cast<input::character_code>(u);
        }

        /** @brief True for code points that are control characters rather than text (C0 controls and DEL). */
        bool is_control_character(input::character_code cp) noexcept
        {
            return cp < 0x20u || cp == 0x7Fu;
        }

        /** @brief Cursor position of a client-area mouse message. */
        math::vec2<std::int32_t> client_pos_from_lparam(LPARAM lparam) noexcept
        {
            return {static_cast<std::int32_t>(GET_X_LPARAM(lparam)), static_cast<std::int32_t>(GET_Y_LPARAM(lparam))};
        }

        /** @brief Cursor position of a message that reports screen coordinates (wheel messages), converted to client space. */
        math::vec2<std::int32_t> client_pos_from_screen_lparam(HWND hwnd, LPARAM lparam) noexcept
        {
            POINT p{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ScreenToClient(hwnd, &p);
            return {static_cast<std::int32_t>(p.x), static_cast<std::int32_t>(p.y)};
        }

        /** @brief Registers for WM_INPUT mouse messages targeted at @p hwnd (replacing any previous registration). */
        void register_raw_mouse(HWND hwnd) noexcept
        {
            RAWINPUTDEVICE rid{};
            rid.usUsagePage = input::usb_hid_page_generic_desktop;
            rid.usUsage = 0x02; // HID_USAGE_GENERIC_MOUSE
            rid.dwFlags = 0;
            rid.hwndTarget = hwnd;
            RegisterRawInputDevices(&rid, 1, sizeof(rid));
        }

        /** @brief Stops WM_INPUT mouse delivery for the process. */
        void unregister_raw_mouse() noexcept
        {
            RAWINPUTDEVICE rid{};
            rid.usUsagePage = input::usb_hid_page_generic_desktop;
            rid.usUsage = 0x02;
            rid.dwFlags = RIDEV_REMOVE;
            rid.hwndTarget = nullptr;
            RegisterRawInputDevices(&rid, 1, sizeof(rid));
        }

        /** @brief Confines the cursor to the window's client area. */
        void clip_cursor_to_client(HWND hwnd) noexcept
        {
            RECT rc{};
            if (!GetClientRect(hwnd, &rc))
                return;

            POINT tl{rc.left, rc.top};
            POINT br{rc.right, rc.bottom};
            ClientToScreen(hwnd, &tl);
            ClientToScreen(hwnd, &br);

            RECT clip{tl.x, tl.y, br.x, br.y};
            ClipCursor(&clip);
        }

        /**
         * @fn apply_cursor_mode
         * @brief Brings the OS state (cursor clip, raw input registration, cursor image) in line with the window's cursor
         * mode and focus. Called whenever either changes and whenever the window moves or resizes while captured.
         */
        void apply_cursor_mode(window_id id, window_state &ws, bool focused) noexcept
        {
            if (ws.cursor == cursor_mode::captured && focused)
            {
                clip_cursor_to_client(ws.hwnd);
                if (g_raw_mouse_window != id)
                {
                    register_raw_mouse(ws.hwnd);
                    g_raw_mouse_window = id;
                    g_has_last_raw_absolute = false;
                }
            }
            else if (g_raw_mouse_window == id)
            {
                ClipCursor(nullptr);
                unregister_raw_mouse();
                g_raw_mouse_window = 0;
            }

            // The cursor image is chosen in WM_SETCURSOR, which only fires on mouse movement; nudge it so a mode change
            // applies immediately when the cursor is already over the client area.
            POINT p{};
            if (GetCursorPos(&p) && WindowFromPoint(p) == ws.hwnd)
                PostMessageW(ws.hwnd, WM_SETCURSOR, reinterpret_cast<WPARAM>(ws.hwnd), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
        }

        /**
         * @brief apply_cursor_mode() with the focus state taken from GetFocus(). Not usable from WM_KILLFOCUS, where the
         * window still reports as focused; pass the state explicitly there.
         */
        void apply_cursor_mode(window_id id, window_state &ws) noexcept
        {
            apply_cursor_mode(id, ws, GetFocus() == ws.hwnd);
        }

        /**
         * @fn release_held_mouse_buttons
         * @brief Publishes a release for every mouse button the backend thinks is held and drops the capture. Used when the
         * capture is taken away (WM_CAPTURECHANGED) or the window loses focus, so no button is left stuck down.
         */
        void release_held_mouse_buttons(window_id id, const math::vec2<std::int32_t> &pos) noexcept
        {
            const input::mouse_buttons held = g_mouse_buttons_down;
            g_mouse_buttons_down = input::mouse_buttons::none;
            g_mouse_capture_window = 0;

            if (held == input::mouse_buttons::none)
                return;

            const input::key_modifiers mods = current_modifiers();
            for (std::size_t i = 0; i < input::mouse_button_count; ++i)
            {
                const auto b = static_cast<input::mouse_button>(i);
                if (!input::has_button(held, b))
                    continue;

                input::mouse_button_event be;
                be.window = id;
                be.button = b;
                be.action = input::mouse_button_action::release;
                be.position_px = pos;
                be.modifiers = mods;
                enqueue_event(be);
            }
        }

        /**
         * @fn release_held_keys
         * @brief Publishes a release for every key the backend thinks is held. Used on focus loss: Windows delivers the
         * WM_KEYUP for keys released after Alt+Tab to the newly focused application, not to us.
         */
        void release_held_keys(window_id id) noexcept
        {
            if (g_keys_down.none())
                return;

            for (std::size_t i = 1; i < input::key_code_count; ++i)
            {
                if (!g_keys_down.test(i))
                    continue;

                input::key_event ke;
                ke.window = id;
                ke.code = static_cast<input::key_code>(i);
                ke.scancode = 0;
                ke.action = input::key_action::release;
                ke.modifiers = input::key_modifiers::none;
                enqueue_event(ke);
            }
            g_keys_down.reset();
        }

        /**
         * @fn publish_text
         * @brief Publishes one code point as a text_input_event, unless it is a control character.
         */
        void publish_text(window_id id, input::character_code cp) noexcept
        {
            if (cp == 0 || is_control_character(cp))
                return;

            input::text_input_event te(cp);
            te.window = id;
            te.modifiers = current_modifiers();
            enqueue_event(te);
        }

        /**
         * @fn window_proc
         * @brief The window procedure for every window this backend creates. Translates window and input messages into
         * Catalyst events (see enqueue_event) and forwards everything else to DefWindowProcW.
         * @note Messages that arrive before CreateWindowExW returns (WM_NCCREATE, WM_CREATE, the first WM_SIZE...) see a
         * window id of 0 because GWLP_USERDATA has not been set yet; those are deliberately not translated, and
         * create_window publishes the initial resize/DPI events itself.
         */
        LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
        {
            const auto id = static_cast<window_id>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            window_state *ws = (id != 0) ? window_state_from_id(id) : nullptr;

            switch (msg)
            {
            // ---- Window lifecycle -------------------------------------------------------------------------------
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

                    if (ws && ws->cursor == cursor_mode::captured)
                        apply_cursor_mode(id, *ws);
                }
                break;
            }
            case WM_MOVE:
            {
                if (ws && ws->cursor == cursor_mode::captured)
                    apply_cursor_mode(id, *ws);
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

            // ---- Focus ------------------------------------------------------------------------------------------
            case WM_SETFOCUS:
            {
                if (id != 0)
                {
                    window_focus_event fe;
                    fe.window = id;
                    fe.focused = true;
                    enqueue_event(fe);

                    if (ws)
                        apply_cursor_mode(id, *ws, true);
                }
                break;
            }
            case WM_KILLFOCUS:
            {
                if (id != 0)
                {
                    release_held_keys(id);
                    release_held_mouse_buttons(id, ws ? ws->last_mouse_pos_px : math::vec2<std::int32_t>{});

                    if (ws)
                        apply_cursor_mode(id, *ws, false); // suspends the clip / raw input while unfocused

                    window_focus_event fe;
                    fe.window = id;
                    fe.focused = false;
                    enqueue_event(fe);
                }
                break;
            }
            case WM_CAPTURECHANGED:
            {
                // Another window (possibly in another process) took the capture: the button-up messages will never
                // reach us, so report the buttons as released now.
                if (id != 0 && reinterpret_cast<HWND>(lparam) != hwnd && g_mouse_capture_window == id)
                    release_held_mouse_buttons(id, ws ? ws->last_mouse_pos_px : math::vec2<std::int32_t>{});
                break;
            }
            case WM_SYSCOMMAND:
            {
                // Alt or F10 on their own would enter the menu loop and stall the message pump; Alt+F4 (SC_CLOSE) and
                // the rest still go through.
                if ((wparam & 0xFFF0u) == SC_KEYMENU)
                    return 0;
                break;
            }
            case WM_MENUCHAR:
            {
                // Silence the beep for Alt+<key> combinations, which have no menu to accelerate.
                return MAKELRESULT(0, MNC_CLOSE);
            }
            case WM_SETCURSOR:
            {
                if (ws && LOWORD(lparam) == HTCLIENT && ws->cursor != cursor_mode::normal)
                {
                    SetCursor(nullptr);
                    return TRUE;
                }
                break;
            }

            // ---- Keyboard ---------------------------------------------------------------------------------------
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            {
                if (id != 0)
                {
                    const bool repeat = (lparam & (1 << 30)) != 0;

                    input::key_event ke;
                    ke.window = id;
                    ke.code = to_input_key_code(wparam, lparam, ke.scancode);
                    ke.action = repeat ? input::key_action::repeat : input::key_action::press;
                    ke.modifiers = current_modifiers();

                    // Phantom Shift messages around keypad keys map to unknown with a Shift virtual key; drop them.
                    if (ke.code == input::key_code::unknown && wparam == VK_SHIFT)
                        break;

                    if (ke.code != input::key_code::unknown)
                        g_keys_down.set(static_cast<std::size_t>(ke.code));

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
                    ke.window = id;
                    ke.code = to_input_key_code(wparam, lparam, ke.scancode);
                    ke.action = input::key_action::release;
                    ke.modifiers = current_modifiers();

                    if (ke.code == input::key_code::unknown && wparam == VK_SHIFT)
                        break;

                    if (ke.code != input::key_code::unknown)
                        g_keys_down.reset(static_cast<std::size_t>(ke.code));

                    enqueue_event(ke);
                }
                break;
            }
            case WM_CHAR:
            {
                if (ws)
                    publish_text(id, utf32_from_utf16_unit(*ws, static_cast<wchar_t>(wparam)));
                break;
            }
            case WM_UNICHAR:
            {
                // Some input tools send UTF-32 directly; answering TRUE to UNICODE_NOCHAR tells them we accept it.
                if (wparam == UNICODE_NOCHAR)
                    return TRUE;
                if (ws)
                    publish_text(id, static_cast<input::character_code>(wparam));
                return 0;
            }

            // ---- Mouse ------------------------------------------------------------------------------------------
            case WM_MOUSEMOVE:
            {
                if (ws)
                {
                    const math::vec2<std::int32_t> pos = client_pos_from_lparam(lparam);

                    if (!ws->mouse_inside)
                    {
                        ws->mouse_inside = true;

                        TRACKMOUSEEVENT tme{};
                        tme.cbSize = sizeof(tme);
                        tme.dwFlags = TME_LEAVE;
                        tme.hwndTrack = hwnd;
                        TrackMouseEvent(&tme);

                        input::mouse_enter_event ee;
                        ee.window = id;
                        ee.position_px = pos;
                        enqueue_event(ee);
                    }

                    const math::vec2<std::int32_t> last = ws->has_mouse_pos ? ws->last_mouse_pos_px : pos;
                    ws->last_mouse_pos_px = pos;
                    ws->has_mouse_pos = true;

                    input::mouse_move_event me;
                    me.window = id;
                    me.position_px = pos;
                    me.delta_px = {pos.x - last.x, pos.y - last.y};
                    me.buttons = buttons_from_wparam(wparam);
                    me.modifiers = current_modifiers();
                    enqueue_event(me);
                }
                break;
            }
            case WM_MOUSELEAVE:
            {
                if (ws && ws->mouse_inside)
                {
                    ws->mouse_inside = false;

                    input::mouse_leave_event le;
                    le.window = id;
                    enqueue_event(le);
                }
                break;
            }
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_XBUTTONDOWN:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDBLCLK:
            case WM_XBUTTONDBLCLK:
            {
                if (ws)
                {
                    const bool double_click = (msg == WM_LBUTTONDBLCLK || msg == WM_RBUTTONDBLCLK ||
                                               msg == WM_MBUTTONDBLCLK || msg == WM_XBUTTONDBLCLK);

                    input::mouse_button_event be;
                    be.window = id;
                    be.button = to_input_mouse_button(msg, wparam);
                    be.action = input::mouse_button_action::press;
                    be.clicks = double_click ? 2 : 1;
                    be.position_px = client_pos_from_lparam(lparam);
                    be.modifiers = current_modifiers();

                    // Capture on the first button so the matching release arrives even if the cursor leaves the window.
                    if (g_mouse_buttons_down == input::mouse_buttons::none)
                    {
                        SetCapture(hwnd);
                        g_mouse_capture_window = id;
                    }
                    g_mouse_buttons_down |= input::to_mouse_buttons(be.button);

                    enqueue_event(be);
                }
                if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK)
                    return TRUE;
                break;
            }
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
            case WM_XBUTTONUP:
            {
                if (ws)
                {
                    input::mouse_button_event be;
                    be.window = id;
                    be.button = to_input_mouse_button(msg, wparam);
                    be.action = input::mouse_button_action::release;
                    be.clicks = 1;
                    be.position_px = client_pos_from_lparam(lparam);
                    be.modifiers = current_modifiers();

                    g_mouse_buttons_down &= ~input::to_mouse_buttons(be.button);
                    if (g_mouse_buttons_down == input::mouse_buttons::none && g_mouse_capture_window == id)
                    {
                        g_mouse_capture_window = 0;
                        ReleaseCapture();
                    }

                    enqueue_event(be);
                }
                if (msg == WM_XBUTTONUP)
                    return TRUE;
                break;
            }
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
            {
                if (ws)
                {
                    const float notches = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / static_cast<float>(WHEEL_DELTA);

                    input::mouse_wheel_event we;
                    we.window = id;
                    we.position_px = client_pos_from_screen_lparam(hwnd, lparam);
                    we.delta = (msg == WM_MOUSEWHEEL) ? math::vec2<float>{0.0f, notches} : math::vec2<float>{notches, 0.0f};
                    we.modifiers = current_modifiers();
                    enqueue_event(we);
                }
                break;
            }
            case WM_INPUT:
            {
                if (ws && ws->cursor == cursor_mode::captured && g_raw_mouse_window == id)
                {
                    RAWINPUT raw{};
                    UINT size = sizeof(raw);
                    const UINT got = GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, &raw, &size,
                                                     sizeof(RAWINPUTHEADER));
                    if (got != static_cast<UINT>(-1) && raw.header.dwType == RIM_TYPEMOUSE)
                    {
                        const RAWMOUSE &m = raw.data.mouse;
                        math::vec2<std::int32_t> delta{};

                        if ((m.usFlags & MOUSE_MOVE_ABSOLUTE) != 0)
                        {
                            // Remote desktop / tablets report absolute positions; turn them into deltas ourselves.
                            const math::vec2<std::int32_t> abs{static_cast<std::int32_t>(m.lLastX), static_cast<std::int32_t>(m.lLastY)};
                            if (g_has_last_raw_absolute)
                                delta = {abs.x - g_last_raw_absolute.x, abs.y - g_last_raw_absolute.y};
                            g_last_raw_absolute = abs;
                            g_has_last_raw_absolute = true;
                        }
                        else
                        {
                            delta = {static_cast<std::int32_t>(m.lLastX), static_cast<std::int32_t>(m.lLastY)};
                        }

                        if (delta.x != 0 || delta.y != 0)
                        {
                            input::mouse_raw_move_event re;
                            re.window = id;
                            re.delta = delta;
                            enqueue_event(re);
                        }
                    }
                }
                break; // DefWindowProc must see WM_INPUT so the system can clean up
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
            wc.style = CS_DBLCLKS; // deliver WM_*BUTTONDBLCLK so mouse_button_event::clicks can report double-clicks
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
            return it->second.hwnd;
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
        window_state ws;
        ws.hwnd = hwnd;
        g_windows.emplace(id, ws);

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
        if (GetFocus() == hwnd)
        {
            // WM_SETFOCUS was delivered before the window id was attached to the HWND, so report it now.
            window_focus_event e;
            e.window = id;
            e.focused = true;
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

        if (g_raw_mouse_window == id)
        {
            ClipCursor(nullptr);
            unregister_raw_mouse();
            g_raw_mouse_window = 0;
        }
        if (g_mouse_capture_window == id)
        {
            g_mouse_capture_window = 0;
            g_mouse_buttons_down = input::mouse_buttons::none;
        }

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

    void set_cursor_mode(window_id id, cursor_mode mode) noexcept
    {
        window_state *ws = window_state_from_id(id);
        if (!ws)
            return;

        if (ws->cursor == mode)
            return;

        ws->cursor = mode;
        apply_cursor_mode(id, *ws);
    }

    cursor_mode get_cursor_mode(window_id id) noexcept
    {
        const window_state *ws = window_state_from_id(id);
        return ws ? ws->cursor : cursor_mode::normal;
    }

} // namespace catalyst::platform::detail
