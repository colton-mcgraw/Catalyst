/**
 * @file clipboard_backend_null.cpp
 * @brief The headless implementation of the clipboard: a process-wide string in memory.
 * @details Unlike the dialogs, which need a person and so report unsupported, a clipboard is just storage, and keeping
 * one in memory lets copy and paste round-trip within the process. That is what code built on the clipboard -- a text
 * field's cut, copy and paste -- needs in order to be tested on a machine with no window system. Nothing is shared
 * with other processes. The clipboard starts with no text, as a fresh desktop session's does. License: MIT (see
 * LICENSE).
 */

#include "../detail_backend.hpp"

#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace catalyst::platform::detail
{
    namespace
    {
        /** @brief The clipboard: no value until something is copied. Guarded by clipboard_mutex(). */
        std::optional<std::string> &clipboard_contents()
        {
            static std::optional<std::string> contents;
            return contents;
        }

        std::mutex &clipboard_mutex()
        {
            static std::mutex mutex;
            return mutex;
        }
    } // namespace

    std::expected<std::string, clipboard_error> get_clipboard_text()
    {
        const std::scoped_lock lock(clipboard_mutex());
        if (!clipboard_contents())
            return std::unexpected(clipboard_error::no_text);
        return *clipboard_contents();
    }

    std::expected<void, clipboard_error> set_clipboard_text(std::string_view text)
    {
        const std::scoped_lock lock(clipboard_mutex());
        clipboard_contents().emplace(text);
        return {};
    }

    bool has_clipboard_text() noexcept
    {
        const std::scoped_lock lock(clipboard_mutex());
        return clipboard_contents().has_value();
    }

} // namespace catalyst::platform::detail
