/**
 * @file clipboard_text.hpp
 * @brief Internal: the backend-independent half of the clipboard -- the text normalisation every backend shares.
 * @details clipboard.hpp promises the application one line-break convention ("\n") and text that ends at the first
 * NUL, on every backend. Rather than have each backend honour that separately, platform::get_clipboard_text and
 * platform::set_clipboard_text do it in clipboard.cpp around the detail:: calls, and a backend only converts between
 * "\n" and its own convention where it has one (Win32's "\r\n", via to_crlf_line_breaks).
 *
 * Everything here is pure string work with no operating-system dependency, defined in clipboard.cpp, so the tests can
 * check it on any backend. License: MIT (see LICENSE).
 */

#pragma once

#include <string>
#include <string_view>

namespace catalyst::platform::detail
{

    /**
     * @fn truncate_at_nul
     * @brief @p text up to, not including, its first NUL; all of it if it has none.
     */
    [[nodiscard]] constexpr std::string_view truncate_at_nul(std::string_view text) noexcept
    {
        return text.substr(0, text.find('\0'));
    }

    /**
     * @fn to_lf_line_breaks
     * @brief @p text with every "\r\n" and every lone "\r" replaced by "\n". Everything else is copied unchanged.
     * @details Run on whatever a backend reads off the clipboard, which may have been written by any program with any
     * convention, including the classic Mac one of bare "\r".
     */
    [[nodiscard]] std::string to_lf_line_breaks(std::string_view text);

    /**
     * @fn to_crlf_line_breaks
     * @brief @p text with every "\n" not already preceded by "\r" replaced by "\r\n".
     * @details For a backend whose platform convention is "\r\n". Its input has been through to_lf_line_breaks, so in
     * practice every "\n" is expanded, but an existing "\r\n" is left alone so the function is safe on any input.
     */
    [[nodiscard]] std::string to_crlf_line_breaks(std::string_view text);

} // namespace catalyst::platform::detail
