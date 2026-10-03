/**
 * @file dialog.cpp
 * @brief The backend-independent half of the native dialogs: the public entry points declared in dialog.hpp, and the
 * normalisation (dialog_spec.hpp) that turns an application's lenient file_dialog_desc into the canonical one every
 * backend's show_file_dialog consumes. License: MIT (see LICENSE).
 */

#include <catalyst/platform/dialog.hpp>

#include "detail_backend.hpp"
#include "dialog_spec.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace catalyst::platform::detail
{
    namespace
    {
        /** @brief Whether @p c separates two patterns in a file_filter::patterns string. */
        constexpr bool is_pattern_separator(char c) noexcept
        {
            return c == ';' || c == ',' || c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
        }

        /** @brief Whether @p text contains a wildcard character. */
        constexpr bool has_wildcard(std::string_view text) noexcept
        {
            return text.find_first_of("*?") != std::string_view::npos;
        }

        /** @brief @p text without leading and trailing ASCII whitespace. */
        constexpr std::string_view trim(std::string_view text) noexcept
        {
            constexpr std::string_view whitespace = " \t\r\n\v\f";
            const std::size_t first = text.find_first_not_of(whitespace);
            if (first == std::string_view::npos)
                return {};
            const std::size_t last = text.find_last_not_of(whitespace);
            return text.substr(first, last - first + 1);
        }

        /** @brief The single file dialog result open_file_dialog and friends return, out of the list a backend gives.
         */
        std::expected<std::filesystem::path, dialog_error>
        single_result(std::expected<std::vector<std::filesystem::path>, dialog_error> result)
        {
            if (!result)
                return std::unexpected(result.error());
            if (result->empty())
                return std::unexpected(dialog_error::failed);
            return std::move(result->front());
        }
    } // namespace

    std::vector<std::string> split_filter_patterns(std::string_view patterns)
    {
        std::vector<std::string> out;

        std::size_t i = 0;
        while (i < patterns.size())
        {
            while (i < patterns.size() && is_pattern_separator(patterns[i]))
                ++i;

            const std::size_t start = i;
            while (i < patterns.size() && !is_pattern_separator(patterns[i]))
                ++i;

            std::string_view token = patterns.substr(start, i - start);
            if (token.empty())
                continue;

            if (has_wildcard(token))
            {
                out.emplace_back(token);
                continue;
            }

            // A bare extension: "png" or ".png" (or "..png", which nobody means but which still names png).
            while (!token.empty() && token.front() == '.')
                token.remove_prefix(1);
            if (token.empty())
                continue;

            std::string pattern = "*.";
            pattern += token;
            out.push_back(std::move(pattern));
        }

        if (out.empty())
            out.emplace_back("*");

        return out;
    }

    std::string normalize_filter_patterns(std::string_view patterns)
    {
        const std::vector<std::string> split = split_filter_patterns(patterns);

        std::string joined;
        for (const std::string &pattern : split)
        {
            if (!joined.empty())
                joined += ';';
            joined += pattern;
        }
        return joined;
    }

    std::string normalize_extension(std::string_view extension)
    {
        extension = trim(extension);

        if (extension.starts_with('*'))
            extension.remove_prefix(1);
        while (!extension.empty() && extension.front() == '.')
            extension.remove_prefix(1);

        if (extension.find_first_of("*?/\\ \t\r\n\v\f") != std::string_view::npos)
            return {};

        return std::string{extension};
    }

    std::string extension_of_pattern(std::string_view pattern)
    {
        pattern = trim(pattern);

        if (!pattern.starts_with("*."))
            return {};
        pattern.remove_prefix(2);

        // "*.*" and "*.png*" imply no one extension; neither does "*." with nothing after it.
        if (pattern.empty() || has_wildcard(pattern))
            return {};

        return normalize_extension(pattern);
    }

    file_dialog_desc normalize_file_dialog_desc(const file_dialog_desc &desc)
    {
        file_dialog_desc out = desc;

        for (file_filter &filter : out.filters)
        {
            filter.patterns = normalize_filter_patterns(filter.patterns);
            if (trim(filter.name).empty())
                filter.name = filter.patterns;
        }

        if (out.default_filter >= out.filters.size())
            out.default_filter = 0;

        out.default_extension = normalize_extension(desc.default_extension);
        if (out.default_extension.empty() && !out.filters.empty())
        {
            const std::vector<std::string> patterns = split_filter_patterns(out.filters[out.default_filter].patterns);
            out.default_extension = extension_of_pattern(patterns.front());
        }

        return out;
    }
} // namespace catalyst::platform::detail

namespace catalyst::platform
{
    std::expected<std::filesystem::path, dialog_error> open_file_dialog(const file_dialog_desc &desc)
    {
        return detail::single_result(
            detail::show_file_dialog(detail::file_dialog_kind::open, detail::normalize_file_dialog_desc(desc)));
    }

    std::expected<std::vector<std::filesystem::path>, dialog_error> open_files_dialog(const file_dialog_desc &desc)
    {
        auto result =
            detail::show_file_dialog(detail::file_dialog_kind::open_multiple, detail::normalize_file_dialog_desc(desc));
        if (result && result->empty())
            return std::unexpected(dialog_error::failed);
        return result;
    }

    std::expected<std::filesystem::path, dialog_error> save_file_dialog(const file_dialog_desc &desc)
    {
        return detail::single_result(
            detail::show_file_dialog(detail::file_dialog_kind::save, detail::normalize_file_dialog_desc(desc)));
    }

    std::expected<std::filesystem::path, dialog_error> pick_folder_dialog(const file_dialog_desc &desc)
    {
        return detail::single_result(
            detail::show_file_dialog(detail::file_dialog_kind::pick_folder, detail::normalize_file_dialog_desc(desc)));
    }

    std::expected<message_box_result, dialog_error> show_message_box(const message_box_desc &desc)
    {
        return detail::show_message_box(desc);
    }
} // namespace catalyst::platform
