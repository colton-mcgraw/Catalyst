/**
 * @file clipboard_backend_win32.cpp
 * @brief The Win32 implementation of the clipboard: CF_UNICODETEXT through OpenClipboard, GetClipboardData and
 * SetClipboardData.
 * @details Only CF_UNICODETEXT is read or written. Windows synthesises it from CF_TEXT and CF_OEMTEXT for programs
 * that only write those, and synthesises them from it for programs that only read them, so one format covers every
 * program.
 *
 * The clipboard is a system-wide lock: OpenClipboard fails with ERROR_ACCESS_DENIED while another program has it open
 * with an owner window, which clipboard managers and remote-desktop clients do briefly every time it changes. Both
 * calls therefore retry for up to open_budget before reporting clipboard_error::busy.
 *
 * Both calls open the clipboard with an owner window, reading included. Writing needs one because SetClipboardData is
 * documented to fail after EmptyClipboard on a clipboard opened with no owner. Reading needs one because, as measured
 * on Windows 11, an open with a null owner does not exclude anyone: another program could empty the clipboard, freeing
 * the memory being read, while it is still being read. The owner is a message-only window created for the call and
 * destroyed at the end of it, on the calling thread, which is what lets these functions be called from any thread and
 * without the application's windows: a long-lived hidden owner would be sent WM_DESTROYCLIPBOARD by the next program
 * to copy something, and on a thread that never pumps messages that send would stall the other program. Data that is
 * not delay-rendered -- and nothing here is -- belongs to the system once SetClipboardData succeeds, so it survives its
 * owner being destroyed, exactly as text copied from a program survives that program being closed.
 * License: MIT (see LICENSE).
 */

#include "../clipboard_text.hpp"
#include "../detail_backend.hpp"
#include "win32_helpers.hpp"

#include <win32/windows_lean.hpp>

#include <chrono>
#include <cstring>
#include <cwchar>
#include <string>
#include <string_view>

namespace catalyst::platform::detail
{
    namespace
    {
        /**
         * @brief How long OpenClipboard is retried while another program holds the clipboard.
         * @details Long enough for the brief opens of clipboard managers and remote-desktop clients, short enough not
         * to be a visible hitch on the keypress that asked. Measured against a clock rather than counted in attempts,
         * because Sleep(1) lasts a whole scheduler tick -- 15.6 ms by default -- so a count of short sleeps would last
         * anywhere from a few to a hundred milliseconds depending on the system timer resolution.
         */
        constexpr std::chrono::milliseconds open_budget{50};

        /**
         * @brief Opens the clipboard for @p owner, retrying while another program holds it.
         * @return Nothing once it is open -- the caller must CloseClipboard -- or busy / failed.
         */
        std::expected<void, clipboard_error> open_clipboard(HWND owner) noexcept
        {
            const auto deadline = std::chrono::steady_clock::now() + open_budget;
            for (;;)
            {
                if (::OpenClipboard(owner))
                    return {};
                if (::GetLastError() != ERROR_ACCESS_DENIED)
                    return std::unexpected(clipboard_error::failed); // Not contention, so waiting will not help.
                if (std::chrono::steady_clock::now() >= deadline)
                    return std::unexpected(clipboard_error::busy);
                ::Sleep(1);
            }
        }

        /** @brief Closes the clipboard when it goes out of scope. Construct only after a successful open_clipboard. */
        struct clipboard_session
        {
            clipboard_session() noexcept = default;
            ~clipboard_session() { ::CloseClipboard(); }
            clipboard_session(const clipboard_session &) = delete;
            clipboard_session &operator=(const clipboard_session &) = delete;
        };

        /** @brief Unlocks a global memory block when it goes out of scope. */
        struct global_lock
        {
            explicit global_lock(HGLOBAL memory) noexcept : memory(memory), data(::GlobalLock(memory)) {}
            ~global_lock()
            {
                if (data)
                    ::GlobalUnlock(memory);
            }
            global_lock(const global_lock &) = delete;
            global_lock &operator=(const global_lock &) = delete;

            HGLOBAL memory;
            void *data;
        };

        /** @brief A message-only window to own the clipboard for one write, destroyed when it goes out of scope. */
        struct owner_window
        {
            owner_window() noexcept
                : hwnd(::CreateWindowExW(0, L"Message", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                         ::GetModuleHandleW(nullptr), nullptr))
            {
            }
            ~owner_window()
            {
                if (hwnd)
                    ::DestroyWindow(hwnd);
            }
            owner_window(const owner_window &) = delete;
            owner_window &operator=(const owner_window &) = delete;

            HWND hwnd;
        };
    } // namespace

    std::expected<std::string, clipboard_error> get_clipboard_text()
    {
        // Opened with an owner even though nothing is written: see the file description for why a null owner would
        // let another program free the data while it is being read.
        const owner_window owner;
        if (!owner.hwnd)
            return std::unexpected(clipboard_error::failed);
        if (auto opened = open_clipboard(owner.hwnd); !opened)
            return std::unexpected(opened.error());
        const clipboard_session session;

        HANDLE handle = ::GetClipboardData(CF_UNICODETEXT);
        if (!handle)
        {
            // Absent, or present but its (delay-rendering) owner could not produce it.
            return std::unexpected(::IsClipboardFormatAvailable(CF_UNICODETEXT) ? clipboard_error::failed
                                                                                : clipboard_error::no_text);
        }

        const global_lock lock(handle);
        if (!lock.data)
            return std::unexpected(clipboard_error::failed);

        // The text should be NUL-terminated, but it was written by another program: never read past the block.
        const auto *text = static_cast<const wchar_t *>(lock.data);
        const std::size_t capacity = ::GlobalSize(handle) / sizeof(wchar_t);
        const std::size_t length = ::wcsnlen(text, capacity);

        // Ill-formed UTF-16 (a lone surrogate) converts with U+FFFD in its place rather than failing.
        return win32::wide_to_utf8(std::wstring_view{text, length});
    }

    std::expected<void, clipboard_error> set_clipboard_text(std::string_view text)
    {
        const std::wstring wide = win32::utf8_to_wide_or_ansi(to_crlf_line_breaks(text));
        if (wide.empty() && !text.empty())
            return std::unexpected(clipboard_error::failed); // Neither UTF-8 nor the legacy code page could read it.

        // Prepare the data before taking the clipboard, so it is held for as short a time as possible.
        const std::size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!memory)
            return std::unexpected(clipboard_error::failed);
        {
            const global_lock lock(memory);
            if (!lock.data)
            {
                ::GlobalFree(memory);
                return std::unexpected(clipboard_error::failed);
            }
            std::memcpy(lock.data, wide.c_str(), bytes); // c_str() includes the terminating NUL.
        }

        const owner_window owner;
        if (!owner.hwnd)
        {
            ::GlobalFree(memory);
            return std::unexpected(clipboard_error::failed);
        }

        if (auto opened = open_clipboard(owner.hwnd); !opened)
        {
            ::GlobalFree(memory);
            return std::unexpected(opened.error());
        }

        // Closed before the owner is destroyed: members and locals are destroyed in reverse order.
        const clipboard_session session;

        if (!::EmptyClipboard())
        {
            ::GlobalFree(memory);
            return std::unexpected(clipboard_error::failed);
        }

        // On success the system owns the memory and it must not be freed; on failure it is still ours.
        if (!::SetClipboardData(CF_UNICODETEXT, memory))
        {
            ::GlobalFree(memory);
            return std::unexpected(clipboard_error::failed);
        }
        return {};
    }

    bool has_clipboard_text() noexcept
    {
        // Needs no OpenClipboard, and reports CF_UNICODETEXT that Windows would synthesise from CF_TEXT as available.
        return ::IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE;
    }

} // namespace catalyst::platform::detail
