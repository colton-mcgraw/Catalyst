#include "win32_helpers.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // For GetModuleHandleW, GetProcAddress, MultiByteToWideChar, WideCharToMultiByte, etc.

#include <cstring>

namespace catalyst::platform::detail::win32
{
    std::wstring utf8_to_wide_or_ansi(const char *str)
    {
        if (!str)
            return L"";

        const int utf8_len = static_cast<int>(std::strlen(str));
        if (utf8_len == 0)
            return L"";

        UINT codepage = CP_UTF8;
        DWORD flags = MB_ERR_INVALID_CHARS;
        int wide_len = MultiByteToWideChar(codepage, flags, str, utf8_len, nullptr, 0);
        if (wide_len == 0)
        {
            codepage = CP_ACP;
            flags = 0;
            wide_len = MultiByteToWideChar(codepage, flags, str, utf8_len, nullptr, 0);
        }

        if (wide_len <= 0)
            return L"";

        std::wstring buffer;
        buffer.resize(static_cast<std::size_t>(wide_len));
        MultiByteToWideChar(codepage, flags, str, utf8_len, buffer.data(), wide_len);
        return buffer;
    }

    std::string wide_to_utf8(const wchar_t *w)
    {
        if (!w || !*w)
            return {};

        const int len = static_cast<int>(wcslen(w));
        const int needed = WideCharToMultiByte(CP_UTF8, 0, w, len, nullptr, 0, nullptr, nullptr);
        if (needed <= 0)
            return {};

        std::string out;
        out.resize(static_cast<std::size_t>(needed));
        WideCharToMultiByte(CP_UTF8, 0, w, len, out.data(), needed, nullptr, nullptr);
        return out;
    }

    float effective_dpi_for_system() noexcept
    {
        using get_dpi_for_system_fn = UINT(WINAPI *)();
        static const auto fn = reinterpret_cast<get_dpi_for_system_fn>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForSystem"));

        if (!fn)
            return 96.0f;

        const UINT dpi = fn();
        if (dpi == 0)
            return 96.0f;

        return static_cast<float>(dpi);
    }

    float effective_dpi_for_window(hwnd_handle hwnd) noexcept
    {
        using get_dpi_for_window_fn = UINT(WINAPI *)(HWND);
        static const auto fn = reinterpret_cast<get_dpi_for_window_fn>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));

        const HWND hwnd_w = reinterpret_cast<HWND>(hwnd);
        if (!fn || hwnd_w == nullptr)
            return effective_dpi_for_system();

        const UINT dpi = fn(hwnd_w);
        if (dpi == 0)
            return effective_dpi_for_system();

        return static_cast<float>(dpi);
    }

    bool try_effective_dpi_for_monitor(hmonitor_handle mon, float &dpi_x, float &dpi_y) noexcept
    {
        // Prefer GetDpiForMonitor (Win 8.1+). Dynamically load to keep compatibility.
        // Signature: HRESULT GetDpiForMonitor(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*)
        using get_dpi_for_monitor_fn = HRESULT(WINAPI *)(HMONITOR, int, UINT *, UINT *);

        static const HMODULE shcore = LoadLibraryW(L"shcore.dll");
        static const auto fn = shcore ? reinterpret_cast<get_dpi_for_monitor_fn>(GetProcAddress(shcore, "GetDpiForMonitor")) : nullptr;

        const HMONITOR mon_w = reinterpret_cast<HMONITOR>(mon);
        if (!fn || mon_w == nullptr)
            return false;

        constexpr int MDT_EFFECTIVE_DPI = 0;
        UINT x = 0;
        UINT y = 0;
        if (FAILED(fn(mon_w, MDT_EFFECTIVE_DPI, &x, &y)) || x == 0 || y == 0)
            return false;

        dpi_x = static_cast<float>(x);
        dpi_y = static_cast<float>(y);
        return true;
    }
} // namespace catalyst::platform::detail::win32
