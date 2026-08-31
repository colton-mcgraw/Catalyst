#pragma once

#include <string>

namespace catalyst::platform::detail::win32
{
    // Opaque handles to avoid including <windows.h> in the header.
    // These are ABI-compatible with Win32's HWND/HMONITOR (both are pointer types).
    using hwnd_handle = void *;
    using hmonitor_handle = void *;

    [[nodiscard]] std::wstring utf8_to_wide_or_ansi(const char *str);
    [[nodiscard]] std::string wide_to_utf8(const wchar_t *w);

    // Returns the system effective DPI (px/in). Falls back to 96.
    [[nodiscard]] float effective_dpi_for_system() noexcept;

    // Returns the window effective DPI (px/in). Falls back to 96.
    [[nodiscard]] float effective_dpi_for_window(hwnd_handle hwnd) noexcept;

    // Attempts to read the monitor effective DPI (px/in). Returns true on success.
    [[nodiscard]] bool try_effective_dpi_for_monitor(hmonitor_handle mon, float &dpi_x, float &dpi_y) noexcept;
} // namespace catalyst::platform::detail::win32
