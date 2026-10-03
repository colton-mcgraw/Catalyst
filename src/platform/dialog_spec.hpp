/**
 * @file dialog_spec.hpp
 * @brief Internal: the backend-independent half of the native dialogs -- turning the lenient file_dialog_desc an
 * application writes into the strict one a backend consumes.
 * @details The public description accepts "png", ".png" and "*.png" alike, separators of ';', ',' or whitespace, an
 * out-of-range default filter and an extension with or without its dot. Accepting all of that in every backend would
 * mean every backend re-implementing the same forgiving parse, and the next one (a GTK portal, an NSOpenPanel) getting
 * it subtly different. So platform::open_file_dialog and its siblings pass the description through
 * normalize_file_dialog_desc first, and detail::show_file_dialog receives only the canonical form documented there.
 *
 * Everything here is pure string work with no operating-system dependency, defined in dialog.cpp, so the tests can
 * check it on any backend. License: MIT (see LICENSE).
 */

#pragma once

#include <catalyst/platform/dialog.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace catalyst::platform::detail
{

    /**
     * @fn split_filter_patterns
     * @brief Splits a file_filter::patterns string into canonical wildcard patterns.
     * @details Tokens are separated by ';', ',' or ASCII whitespace; empty tokens are dropped. A token containing '*'
     * or '?' is kept as written. Any other token is a bare extension: its leading dots are stripped and "*." is put in
     * front, so "png" and ".png" both become "*.png". A token that is nothing but dots is dropped. An input with no
     * tokens left yields the single pattern "*", which matches every file.
     * @param patterns The patterns as the application wrote them.
     * @return At least one pattern, in input order. Duplicates are kept.
     */
    [[nodiscard]] std::vector<std::string> split_filter_patterns(std::string_view patterns);

    /**
     * @fn normalize_filter_patterns
     * @brief split_filter_patterns joined back with ';', the separator every native file dialog API understands.
     */
    [[nodiscard]] std::string normalize_filter_patterns(std::string_view patterns);

    /**
     * @fn normalize_extension
     * @brief Reduces an extension as an application might write it -- "png", ".png", "*.png", " png " -- to the bare
     * form "png" that a native dialog expects.
     * @return The extension without dots or surrounding whitespace, or an empty string if what is left contains a
     * wildcard, a path separator or a space, none of which an appended extension may contain.
     */
    [[nodiscard]] std::string normalize_extension(std::string_view extension);

    /**
     * @fn extension_of_pattern
     * @brief The extension a pattern of the form "*.ext" names, as normalize_extension would return it.
     * @return "png" for "*.png" and "tar.gz" for "*.tar.gz"; an empty string for anything else, including "*", "*.*",
     * "*.png*" and "img*.png", where no single extension is implied.
     */
    [[nodiscard]] std::string extension_of_pattern(std::string_view pattern);

    /**
     * @fn normalize_file_dialog_desc
     * @brief Returns a copy of @p desc in the canonical form a backend's show_file_dialog relies on.
     * @details
     * - every filter's patterns are normalize_filter_patterns of the original, so never empty;
     * - every filter has a name: an empty one is replaced by its normalized patterns;
     * - default_filter indexes a filter, or is 0 when there are none;
     * - default_extension is normalize_extension of the original, or, when that is empty, extension_of_pattern of the
     *   first pattern of the default filter (which may itself be empty).
     *
     * The owner, title, initial folder and default name are copied unchanged; checking the owner and the folder needs
     * the operating system, so the backend does it.
     */
    [[nodiscard]] file_dialog_desc normalize_file_dialog_desc(const file_dialog_desc &desc);

} // namespace catalyst::platform::detail
