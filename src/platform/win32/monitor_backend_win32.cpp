#include "../detail_backend.hpp"

// For EnumDisplayMonitors, GetMonitorInfoW, CreateDCW, GetDeviceCaps, EnumDisplaySettingsW, etc.
#include "win32_helpers.hpp"

#include <win32/windows_lean.hpp>

#include <algorithm>
#include <array>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace catalyst::platform::detail
{
    namespace
    {
        /**
         * @struct monitor_cache
         * @brief The last enumerated monitor list, kept so the queries below do not re-enumerate on every call.
         * @details Enumerating costs a CreateDCW, an EnumDisplaySettingsW and a DPI query per monitor, which an
         * application that asks for its monitor's refresh rate once per frame would otherwise pay every frame. The
         * cache is dropped from the window backend's WM_DISPLAYCHANGE, WM_DPICHANGED and WM_SETTINGCHANGE handlers
         * (see win32::invalidate_monitor_cache). Those messages only reach a process that owns a window, so a cheap
         * fingerprint of the desktop geometry -- five GetSystemMetrics reads, which are user-mode lookups -- guards the
         * windowless case against a monitor being plugged or unplugged unnoticed. A refresh-rate-only change in a
         * windowless process is the one thing neither catches, and a process with no window has nothing to pace to it.
         *
         * Monitor queries carry no threading guarantee in the public API, so the cache takes a mutex rather than
         * assuming the caller's thread; the lock is uncontended in practice and costs tens of nanoseconds.
         */
        struct monitor_cache
        {
            std::mutex mutex;
            std::vector<monitor_desc> list;
            std::array<int, 5> fingerprint{};
            bool valid = false;
        };

        monitor_cache g_monitor_cache;

        [[nodiscard]] std::array<int, 5> desktop_fingerprint() noexcept
        {
            return {GetSystemMetrics(SM_CMONITORS), GetSystemMetrics(SM_XVIRTUALSCREEN),
                    GetSystemMetrics(SM_YVIRTUALSCREEN), GetSystemMetrics(SM_CXVIRTUALSCREEN),
                    GetSystemMetrics(SM_CYVIRTUALSCREEN)};
        }

        monitor_id monitor_id_from_hmonitor(HMONITOR m) noexcept
        {
            return static_cast<monitor_id>(reinterpret_cast<std::uintptr_t>(m));
        }

        void fill_mm_and_refresh(const wchar_t *device_name, monitor_desc &out) noexcept
        {
            if (!device_name || !*device_name)
                return;

            // Physical size in mm (may be 0 or inaccurate depending on driver/reporting).
            HDC hdc = CreateDCW(L"DISPLAY", device_name, nullptr, nullptr);
            if (hdc)
            {
                const int w_mm = GetDeviceCaps(hdc, HORZSIZE);
                const int h_mm = GetDeviceCaps(hdc, VERTSIZE);
                if (w_mm > 0 && h_mm > 0)
                    out.size_mm = {static_cast<float>(w_mm), static_cast<float>(h_mm)};

                DeleteDC(hdc);
            }

            // Refresh rate.
            DEVMODEW dm{};
            dm.dmSize = sizeof(dm);
            if (EnumDisplaySettingsW(device_name, ENUM_CURRENT_SETTINGS, &dm))
            {
                if (dm.dmDisplayFrequency > 1)
                    out.refresh_rate_hz = static_cast<std::uint32_t>(dm.dmDisplayFrequency);
            }
        }

        void fill_dpi(HMONITOR mon, monitor_desc &out) noexcept
        {
            float dx = 0.0f;
            float dy = 0.0f;
            if (win32::try_effective_dpi_for_monitor(mon, dx, dy))
            {
                out.dpi_x = dx;
                out.dpi_y = dy;
                return;
            }

            const float sys = win32::effective_dpi_for_system();
            out.dpi_x = sys;
            out.dpi_y = sys;
        }

        struct enum_ctx
        {
            std::vector<monitor_desc> *out = nullptr;
        };

        BOOL CALLBACK enum_monitors_proc(HMONITOR mon, HDC, LPRECT, LPARAM user)
        {
            auto &ctx = *reinterpret_cast<enum_ctx *>(user);
            if (!ctx.out)
                return TRUE;

            MONITORINFOEXW mi{};
            mi.cbSize = sizeof(mi);
            if (!GetMonitorInfoW(mon, &mi))
                return TRUE;

            monitor_desc d;
            d.id = monitor_id_from_hmonitor(mon);
            d.name = win32::wide_to_utf8(mi.szDevice);
            d.bounds_px = {{mi.rcMonitor.left, mi.rcMonitor.top}, {mi.rcMonitor.right, mi.rcMonitor.bottom}};
            d.work_area_px = {{mi.rcWork.left, mi.rcWork.top}, {mi.rcWork.right, mi.rcWork.bottom}};
            d.primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;

            fill_mm_and_refresh(mi.szDevice, d);
            fill_dpi(mon, d);

            ctx.out->push_back(std::move(d));
            return TRUE;
        }

        std::vector<monitor_desc> enumerate_monitors()
        {
            std::vector<monitor_desc> out;
            enum_ctx ctx{&out};
            EnumDisplayMonitors(nullptr, nullptr, &enum_monitors_proc, reinterpret_cast<LPARAM>(&ctx));

            // Ensure a stable ordering (primary first, then by id) for deterministic tests/logs.
            std::stable_sort(out.begin(), out.end(),
                             [](const monitor_desc &a, const monitor_desc &b)
                             {
                                 if (a.primary != b.primary)
                                     return a.primary;
                                 return a.id < b.id;
                             });

            return out;
        }

        /**
         * @fn with_monitor_list
         * @brief Runs @p fn against the cached monitor list, re-enumerating first if the cache is invalid or the
         * desktop geometry has changed under it.
         */
        template <typename Fn>
        auto with_monitor_list(Fn &&fn) noexcept -> decltype(fn(std::declval<const std::vector<monitor_desc> &>()))
        {
            std::lock_guard<std::mutex> lock(g_monitor_cache.mutex);

            const std::array<int, 5> now = desktop_fingerprint();
            if (!g_monitor_cache.valid || now != g_monitor_cache.fingerprint)
            {
                g_monitor_cache.list = enumerate_monitors();
                g_monitor_cache.fingerprint = now;
                g_monitor_cache.valid = true;
            }

            return fn(static_cast<const std::vector<monitor_desc> &>(g_monitor_cache.list));
        }
    } // namespace

    namespace win32
    {
        void invalidate_monitor_cache() noexcept
        {
            std::lock_guard<std::mutex> lock(g_monitor_cache.mutex);
            g_monitor_cache.valid = false;
        }
    } // namespace win32

    std::size_t get_monitor_count() noexcept
    {
        return with_monitor_list([](const std::vector<monitor_desc> &list) { return list.size(); });
    }

    std::vector<monitor_desc> get_monitor_list() noexcept
    {
        return with_monitor_list([](const std::vector<monitor_desc> &list) { return list; });
    }

    monitor_desc get_monitor(monitor_id id) noexcept
    {
        return with_monitor_list(
            [id](const std::vector<monitor_desc> &list)
            {
                const auto it =
                    std::find_if(list.begin(), list.end(), [&](const monitor_desc &m) { return m.id == id; });
                return it == list.end() ? monitor_desc{} : *it;
            });
    }

    monitor_id primary_monitor() noexcept
    {
        // The primary monitor is the one containing the desktop origin, which the OS answers directly without any
        // enumeration at all.
        const HMONITOR mon = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
        return mon ? monitor_id_from_hmonitor(mon) : 0;
    }

    monitor_id monitor_for_window(window_id w) noexcept
    {
        const native_handle h = get_native_handle(w);
        if (h.kind != native_handle_kind::win32_hwnd || h.handle == nullptr)
            return 0;

        const HWND hwnd = static_cast<HWND>(h.handle);
        const HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        if (!mon)
            return 0;

        return monitor_id_from_hmonitor(mon);
    }
} // namespace catalyst::platform::detail
