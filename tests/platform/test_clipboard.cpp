/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Tests for the catalyst::platform clipboard, against the null backend.
 * @details The system clipboard is shared with every other program on the machine, so nothing here touches it. What is
 * checked is the half every backend shares -- the line-break and NUL handling in src/platform/clipboard_text.hpp,
 * reached directly because it has no public header -- and the public contract as the null backend's in-process
 * clipboard honours it: no text until something is copied, copies round-trip, an empty copy is text, and line breaks
 * come back as "\n" whatever went in.
 */

#include <catalyst/platform/clipboard.hpp>

#include "../test_common.hpp"

#include <platform/clipboard_text.hpp>

#include <string>
#include <string_view>

namespace
{
    using namespace std::string_view_literals;

    using catalyst::platform::clipboard_error;
    using catalyst::platform::detail::to_crlf_line_breaks;
    using catalyst::platform::detail::to_lf_line_breaks;
    using catalyst::platform::detail::truncate_at_nul;

    void test_line_breaks()
    {
        // Every convention becomes "\n", and "\r\n" counts once.
        CT_REQUIRE(to_lf_line_breaks("a\r\nb") == "a\nb");
        CT_REQUIRE(to_lf_line_breaks("a\rb") == "a\nb");
        CT_REQUIRE(to_lf_line_breaks("a\nb") == "a\nb");
        CT_REQUIRE(to_lf_line_breaks("a\r\n\r\nb") == "a\n\nb");
        CT_REQUIRE(to_lf_line_breaks("a\r\rb") == "a\n\nb");
        CT_REQUIRE(to_lf_line_breaks("a\n\rb") == "a\n\nb"); // "\n\r" is two breaks, not one reversed one.
        CT_REQUIRE(to_lf_line_breaks("\r") == "\n");
        CT_REQUIRE(to_lf_line_breaks("end\r\n") == "end\n");
        CT_REQUIRE(to_lf_line_breaks("").empty());

        // Everything that is not a line break is left alone, including other control characters and UTF-8.
        CT_REQUIRE(to_lf_line_breaks("tab\there") == "tab\there");
        CT_REQUIRE(to_lf_line_breaks("caf\xC3\xA9\r\n") == "caf\xC3\xA9\n");

        // Expansion adds "\r" before a lone "\n" and leaves an existing "\r\n" as it is.
        CT_REQUIRE(to_crlf_line_breaks("a\nb") == "a\r\nb");
        CT_REQUIRE(to_crlf_line_breaks("\n\n") == "\r\n\r\n");
        CT_REQUIRE(to_crlf_line_breaks("a\r\nb") == "a\r\nb");
        CT_REQUIRE(to_crlf_line_breaks("no breaks") == "no breaks");
        CT_REQUIRE(to_crlf_line_breaks("").empty());

        // The two are inverses on canonical text.
        const std::string canonical = "line one\nline two\n\nline four";
        CT_REQUIRE(to_lf_line_breaks(to_crlf_line_breaks(canonical)) == canonical);
    }

    void test_nul()
    {
        CT_REQUIRE(truncate_at_nul("abc"sv) == "abc");
        CT_REQUIRE(truncate_at_nul("ab\0cd"sv) == "ab");
        CT_REQUIRE(truncate_at_nul("\0cd"sv).empty());
        CT_REQUIRE(truncate_at_nul(""sv).empty());
    }

    // Runs first, while the process's in-memory clipboard is still untouched.
    void test_starts_empty()
    {
        CT_REQUIRE(!catalyst::platform::has_clipboard_text());

        const auto text = catalyst::platform::get_clipboard_text();
        CT_REQUIRE(!text.has_value());
        CT_REQUIRE(text.error() == clipboard_error::no_text);
    }

    void test_round_trip()
    {
        CT_REQUIRE(catalyst::platform::set_clipboard_text("hello, clipboard").has_value());
        CT_REQUIRE(catalyst::platform::has_clipboard_text());
        {
            const auto text = catalyst::platform::get_clipboard_text();
            CT_REQUIRE(text.has_value());
            CT_REQUIRE(*text == "hello, clipboard");
        }

        // A second copy replaces the first.
        CT_REQUIRE(catalyst::platform::set_clipboard_text("caf\xC3\xA9 \xE2\x9C\x93").has_value());
        CT_REQUIRE(catalyst::platform::get_clipboard_text().value() == "caf\xC3\xA9 \xE2\x9C\x93");

        // Reading does not consume.
        CT_REQUIRE(catalyst::platform::get_clipboard_text().value() == "caf\xC3\xA9 \xE2\x9C\x93");
    }

    void test_normalisation_through_the_api()
    {
        // Whatever convention is copied, "\n" comes back.
        CT_REQUIRE(catalyst::platform::set_clipboard_text("one\r\ntwo\rthree\nfour").has_value());
        CT_REQUIRE(catalyst::platform::get_clipboard_text().value() == "one\ntwo\nthree\nfour");

        // The text ends at the first NUL.
        CT_REQUIRE(catalyst::platform::set_clipboard_text("kept\0dropped"sv).has_value());
        CT_REQUIRE(catalyst::platform::get_clipboard_text().value() == "kept");
    }

    void test_empty_text_is_text()
    {
        CT_REQUIRE(catalyst::platform::set_clipboard_text("").has_value());
        CT_REQUIRE(catalyst::platform::has_clipboard_text());

        const auto text = catalyst::platform::get_clipboard_text();
        CT_REQUIRE(text.has_value());
        CT_REQUIRE(text->empty());
    }

    void test_error_names()
    {
        CT_REQUIRE(!catalyst::platform::name(clipboard_error::no_text).empty());
        CT_REQUIRE(!catalyst::platform::name(clipboard_error::busy).empty());
        CT_REQUIRE(!catalyst::platform::name(clipboard_error::failed).empty());
        CT_REQUIRE(catalyst::platform::name(clipboard_error::busy) !=
                   catalyst::platform::name(clipboard_error::failed));
    }
} // namespace

int main()
{
    test_line_breaks();
    test_nul();
    test_starts_empty();
    test_round_trip();
    test_normalisation_through_the_api();
    test_empty_text_is_text();
    test_error_names();
    return 0;
}
