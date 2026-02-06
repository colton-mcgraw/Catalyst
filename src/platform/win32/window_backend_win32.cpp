#include <catalyst/platform/window.hpp>

#include <catalyst/core/event_sink.hpp>
#include <catalyst/input/keyboard.hpp>
#include <catalyst/input/mouse.hpp>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include <deque>
#include <cstring>
#include <string>
#include <unordered_map>

namespace catalyst::platform::detail
{

    namespace
    {
        constexpr wchar_t k_window_class_name[] = L"CatalystWindow";

        std::deque<event> g_events;
        std::unordered_map<window_id, HWND> g_windows;
        std::uint64_t g_next_window_id = 1;

        core::event_sink *g_event_sink = nullptr;
        std::unordered_map<window_id, math::vec2<std::int32_t>> g_last_mouse_pos_px;
        thread_local wchar_t g_pending_high_surrogate = 0;

        float dpi_scale_for_window(HWND hwnd) noexcept
        {
            using get_dpi_for_window_fn = UINT(WINAPI *)(HWND);
            static const auto fn = reinterpret_cast<get_dpi_for_window_fn>(
                GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));

            if (!fn)
                return 1.0f;

            const UINT dpi = fn(hwnd);
            if (dpi == 0)
                return 1.0f;

            return static_cast<float>(dpi) / 96.0f;
        }

        void enqueue_event(const event &e)
        {
            g_events.push_back(e);
        }

        input::mouse_button to_input_mouse_button(mouse_button b) noexcept
        {
            switch (b)
            {
            case mouse_button::left:
                return input::mouse_button::left;
            case mouse_button::right:
                return input::mouse_button::right;
            case mouse_button::middle:
                return input::mouse_button::middle;
            case mouse_button::x1:
                return input::mouse_button::x1;
            case mouse_button::x2:
                return input::mouse_button::x2;
            default:
                return input::mouse_button::unknown;
            }
        }

        mouse_button to_mouse_button(UINT msg, WPARAM wparam)
        {
            switch (msg)
            {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                return mouse_button::left;
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
                return mouse_button::right;
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                return mouse_button::middle;
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
                return (GET_XBUTTON_WPARAM(wparam) == XBUTTON1) ? mouse_button::x1 : mouse_button::x2;
            default:
                return mouse_button::left;
            }
        }

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

        LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
        {
            const auto id = static_cast<window_id>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

            switch (msg)
            {
            case WM_CLOSE:
            {
                if (id != 0)
                {
                    event e;
                    e.window = id;
                    e.type = event_type::window_close_requested;
                    enqueue_event(e);
                }
                return 0; // app decides when to destroy
            }
            case WM_DESTROY:
            {
                if (id != 0)
                {
                    event e;
                    e.window = id;
                    e.type = event_type::window_destroyed;
                    enqueue_event(e);
                }
                return 0;
            }
            case WM_SIZE:
            {
                if (id != 0)
                {
                    event e;
                    e.window = id;
                    e.type = event_type::window_resized;
                    e.resized.width_px = LOWORD(lparam);
                    e.resized.height_px = HIWORD(lparam);
                    enqueue_event(e);
                }
                break;
            }
            case WM_ENTERSIZEMOVE:
            {
                if (id != 0)
                {
                    event e;
                    e.window = id;
                    e.type = event_type::window_enter_size_move;
                    enqueue_event(e);
                }
                break;
            }
            case WM_EXITSIZEMOVE:
            {
                if (id != 0)
                {
                    event e;
                    e.window = id;
                    e.type = event_type::window_exit_size_move;
                    enqueue_event(e);
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

                    event e;
                    e.window = id;
                    e.type = event_type::window_dpi_changed;
                    e.dpi.dpi_scale = dpi_scale_for_window(hwnd);
                    enqueue_event(e);
                }
                return 0;
            }

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            {
                if (id != 0)
                {
                    event e;
                    e.window = id;
                    e.type = event_type::key_down;
                    e.key.key = static_cast<std::uint32_t>(wparam);
                    e.key.repeat = (lparam & (1 << 30)) != 0;
                    enqueue_event(e);

                    if (g_event_sink)
                    {
                        input::key_event ke;
                        ke.code = to_input_key_code(msg, wparam, lparam);
                        ke.native_code = input::to_usb_hid(ke.code);
                        ke.action = e.key.repeat ? input::key_action::repeat : input::key_action::press;
                        ke.modifiers = current_modifiers();
                        g_event_sink->publish(ke);
                    }
                }
                break;
            }
            case WM_KEYUP:
            case WM_SYSKEYUP:
            {
                if (id != 0)
                {
                    event e;
                    e.window = id;
                    e.type = event_type::key_up;
                    e.key.key = static_cast<std::uint32_t>(wparam);
                    e.key.repeat = false;
                    enqueue_event(e);

                    if (g_event_sink)
                    {
                        input::key_event ke;
                        ke.code = to_input_key_code(msg, wparam, lparam);
                        ke.native_code = input::to_usb_hid(ke.code);
                        ke.action = input::key_action::release;
                        ke.modifiers = current_modifiers();
                        g_event_sink->publish(ke);
                    }
                }
                break;
            }

            case WM_CHAR:
            {
                if (id != 0 && g_event_sink)
                {
                    const auto cp = utf32_from_utf16_unit(static_cast<wchar_t>(wparam));
                    if (cp != 0)
                    {
                        input::character_event ce;
                        ce.character = cp;
                        ce.modifiers = current_modifiers();
                        g_event_sink->publish(ce);

                        const char32_t one[1] = {static_cast<char32_t>(cp)};
                        input::text_input_event te{std::span<const char32_t>(one, 1)};
                        g_event_sink->publish(te);
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

                    event e;
                    e.window = id;
                    e.type = event_type::mouse_move;
                    e.mouse_move.x_px = x;
                    e.mouse_move.y_px = y;
                    enqueue_event(e);

                    if (g_event_sink)
                    {
                        const math::vec2<std::int32_t> pos{x, y};
                        const auto it = g_last_mouse_pos_px.find(id);
                        const math::vec2<std::int32_t> last = (it != g_last_mouse_pos_px.end()) ? it->second : pos;
                        g_last_mouse_pos_px[id] = pos;

                        input::mouse_move_event me;
                        me.position_px = pos;
                        me.delta_px = {pos.x - last.x, pos.y - last.y};
                        g_event_sink->publish(me);
                    }
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
                    event e;
                    e.window = id;
                    e.type = event_type::mouse_button_down;
                    e.mouse_button.button = to_mouse_button(msg, wparam);
                    enqueue_event(e);

                    if (g_event_sink)
                    {
                        input::mouse_button_event be;
                        be.button = to_input_mouse_button(e.mouse_button.button);
                        be.action = input::mouse_button_action::press;
                        be.position_px = {static_cast<std::int32_t>(GET_X_LPARAM(lparam)), static_cast<std::int32_t>(GET_Y_LPARAM(lparam))};
                        g_event_sink->publish(be);
                    }
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
                    event e;
                    e.window = id;
                    e.type = event_type::mouse_button_up;
                    e.mouse_button.button = to_mouse_button(msg, wparam);
                    enqueue_event(e);

                    if (g_event_sink)
                    {
                        input::mouse_button_event be;
                        be.button = to_input_mouse_button(e.mouse_button.button);
                        be.action = input::mouse_button_action::release;
                        be.position_px = {static_cast<std::int32_t>(GET_X_LPARAM(lparam)), static_cast<std::int32_t>(GET_Y_LPARAM(lparam))};
                        g_event_sink->publish(be);
                    }
                }
                if (msg == WM_XBUTTONUP)
                    return TRUE;
                break;
            }
            case WM_MOUSEWHEEL:
            {
                if (id != 0)
                {
                    event e;
                    e.window = id;
                    e.type = event_type::mouse_wheel;
                    e.wheel.delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / 120.0f;
                    enqueue_event(e);

                    if (g_event_sink)
                    {
                        POINT p{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)}; // screen coords
                        ScreenToClient(hwnd, &p);

                        input::mouse_wheel_event we;
                        we.position_px = {static_cast<std::int32_t>(p.x), static_cast<std::int32_t>(p.y)};
                        we.delta = {0.0f, e.wheel.delta};
                        g_event_sink->publish(we);
                    }
                }
                break;
            }
            default:
                break;
            }

            return DefWindowProcW(hwnd, msg, wparam, lparam);
        }

        void ensure_window_class_registered()
        {
            static bool registered = false;
            if (registered)
                return;

            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = &window_proc;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.lpszClassName = k_window_class_name;
            wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));

            RegisterClassExW(&wc);
            registered = true;
        }

        std::wstring utf8_to_wide_or_ansi(const char *s)
        {
            if (!s)
                return L"";

            const int utf8_len = static_cast<int>(std::strlen(s));
            if (utf8_len == 0)
                return L"";

            // Try UTF-8 first.
            int wide_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, utf8_len, nullptr, 0);
            UINT codepage = CP_UTF8;
            DWORD flags = MB_ERR_INVALID_CHARS;
            if (wide_len == 0)
            {
                // Fallback to ANSI codepage.
                codepage = CP_ACP;
                flags = 0;
                wide_len = MultiByteToWideChar(codepage, flags, s, utf8_len, nullptr, 0);
            }

            if (wide_len <= 0)
                return L"";

            std::wstring w;
            w.resize(static_cast<std::size_t>(wide_len));
            MultiByteToWideChar(codepage, flags, s, utf8_len, w.data(), wide_len);
            return w;
        }

        HWND hwnd_from_id(window_id id) noexcept
        {
            auto it = g_windows.find(id);
            if (it == g_windows.end())
                return nullptr;
            return it->second;
        }
    }

    window_id create_window(const window_desc &desc)
    {
        ensure_window_class_registered();

        const window_id id = g_next_window_id++;

        const DWORD style = desc.resizable ? WS_OVERLAPPEDWINDOW : (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);
        const DWORD ex_style = WS_EX_APPWINDOW;

        std::wstring titleW = desc.title ? utf8_to_wide_or_ansi(desc.title) : L"Catalyst";
        if (titleW.empty())
            titleW = L"Catalyst";

        RECT r{0, 0, static_cast<LONG>(desc.width_px), static_cast<LONG>(desc.height_px)};
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
            event e;
            e.window = id;
            e.type = event_type::window_resized;
            RECT cr{};
            GetClientRect(hwnd, &cr);
            e.resized.width_px = static_cast<std::int32_t>(cr.right - cr.left);
            e.resized.height_px = static_cast<std::int32_t>(cr.bottom - cr.top);
            enqueue_event(e);
        }
        {
            event e;
            e.window = id;
            e.type = event_type::window_dpi_changed;
            e.dpi.dpi_scale = dpi_scale_for_window(hwnd);
            enqueue_event(e);
        }

        return id;
    }

    void destroy_window(window_id id) noexcept
    {
        HWND hwnd = hwnd_from_id(id);
        if (!hwnd)
            return;

        DestroyWindow(hwnd);
        g_windows.erase(id);
    }

    bool is_window_valid(window_id id) noexcept
    {
        HWND hwnd = hwnd_from_id(id);
        return hwnd != nullptr && IsWindow(hwnd);
    }

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

    float dpi_scale(window_id id) noexcept
    {
        HWND hwnd = hwnd_from_id(id);
        if (!hwnd)
            return 1.0f;

        return dpi_scale_for_window(hwnd);
    }

    void pump_events() noexcept
    {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

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
        if (rc == WAIT_FAILED)
            return false;
        return true;
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
        g_event_sink = sink;
    }

} // namespace catalyst::platform::detail
