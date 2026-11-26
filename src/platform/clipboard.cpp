/**
 * @file clipboard.cpp
 * @brief The backend-independent half of the clipboard: the public entry points declared in clipboard.hpp, and the
 * line-break normalisation (clipboard_text.hpp) applied around every backend's detail:: calls. License: MIT (see
 * LICENSE).
 */

#include <catalyst/platform/clipboard.hpp>

#include "clipboard_text.hpp"
#include "detail_backend.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace catalyst::platform::detail
{

    std::string to_lf_line_breaks(std::string_view text)
    {
        std::string out;
        out.reserve(text.size());
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            const char c = text[i];
            if (c != '\r')
            {
                out.push_back(c);
                continue;
            }
            out.push_back('\n');
            if (i + 1 < text.size() && text[i + 1] == '\n')
                ++i; // "\r\n" is one break, not two.
        }
        return out;
    }

    std::string to_crlf_line_breaks(std::string_view text)
    {
        std::string out;
        out.reserve(text.size() + text.size() / 16);
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            const char c = text[i];
            if (c == '\n' && (i == 0 || text[i - 1] != '\r'))
                out.push_back('\r');
            out.push_back(c);
        }
        return out;
    }

} // namespace catalyst::platform::detail

namespace catalyst::platform
{

    std::expected<std::string, clipboard_error> get_clipboard_text()
    {
        auto raw = detail::get_clipboard_text();
        if (!raw)
            return std::unexpected(raw.error());
        return detail::to_lf_line_breaks(detail::truncate_at_nul(*raw));
    }

    std::expected<void, clipboard_error> set_clipboard_text(std::string_view utf8_text)
    {
        const std::string canonical = detail::to_lf_line_breaks(detail::truncate_at_nul(utf8_text));
        return detail::set_clipboard_text(canonical);
    }

    bool has_clipboard_text() noexcept
    {
        return detail::has_clipboard_text();
    }

} // namespace catalyst::platform
