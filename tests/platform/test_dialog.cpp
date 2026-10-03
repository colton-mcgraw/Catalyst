/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Tests for the catalyst::platform native dialogs, against the null backend.
 * @details A native dialog is modal and needs a person to dismiss it, so nothing here shows one. What can be checked
 * headlessly is the half that does not depend on the operating system -- the normalisation that turns an application's
 * file_dialog_desc into the canonical form every backend consumes (src/platform/dialog_spec.hpp, reached directly
 * because it has no public header) -- and the null backend's contract: every dialog answers unsupported, at once.
 */

#include <catalyst/platform/dialog.hpp>
#include <catalyst/platform/window.hpp>

#include "../test_common.hpp"

#include <platform/dialog_spec.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace
{
    using catalyst::platform::detail::extension_of_pattern;
    using catalyst::platform::detail::normalize_extension;
    using catalyst::platform::detail::normalize_file_dialog_desc;
    using catalyst::platform::detail::normalize_filter_patterns;
    using catalyst::platform::detail::split_filter_patterns;

    void test_patterns()
    {
        // Already canonical input is left alone.
        CT_REQUIRE(normalize_filter_patterns("*.png;*.jpg") == "*.png;*.jpg");

        // Bare extensions, with or without the dot, become wildcards.
        CT_REQUIRE(normalize_filter_patterns("png") == "*.png");
        CT_REQUIRE(normalize_filter_patterns(".png") == "*.png");
        CT_REQUIRE(normalize_filter_patterns("png;.jpg;*.gif") == "*.png;*.jpg;*.gif");

        // Commas and whitespace separate too, and runs of separators produce no empty patterns.
        CT_REQUIRE(normalize_filter_patterns("png, jpg") == "*.png;*.jpg");
        CT_REQUIRE(normalize_filter_patterns("  *.png ;; *.jpg\t") == "*.png;*.jpg");

        // Wildcard patterns are kept as written, including ones that are not "*.ext".
        CT_REQUIRE(normalize_filter_patterns("img_??.png;*") == "img_??.png;*");

        // Nothing usable matches everything.
        CT_REQUIRE(normalize_filter_patterns("") == "*");
        CT_REQUIRE(normalize_filter_patterns(" ; , ") == "*");
        CT_REQUIRE(normalize_filter_patterns("...") == "*");

        const std::vector<std::string> split = split_filter_patterns("tar.gz zip");
        CT_REQUIRE(split.size() == 2);
        CT_REQUIRE(split[0] == "*.tar.gz");
        CT_REQUIRE(split[1] == "*.zip");
    }

    void test_extensions()
    {
        CT_REQUIRE(normalize_extension("png") == "png");
        CT_REQUIRE(normalize_extension(".png") == "png");
        CT_REQUIRE(normalize_extension("*.png") == "png");
        CT_REQUIRE(normalize_extension("  png ") == "png");
        CT_REQUIRE(normalize_extension("tar.gz") == "tar.gz");
        CT_REQUIRE(normalize_extension("") == "");
        CT_REQUIRE(normalize_extension("*") == "");
        CT_REQUIRE(normalize_extension("*.*") == "");
        CT_REQUIRE(normalize_extension("p?g") == "");
        CT_REQUIRE(normalize_extension("a/b") == "");
        CT_REQUIRE(normalize_extension("a b") == "");

        CT_REQUIRE(extension_of_pattern("*.png") == "png");
        CT_REQUIRE(extension_of_pattern("*.tar.gz") == "tar.gz");
        CT_REQUIRE(extension_of_pattern("*") == "");
        CT_REQUIRE(extension_of_pattern("*.*") == "");
        CT_REQUIRE(extension_of_pattern("*.") == "");
        CT_REQUIRE(extension_of_pattern("*.png*") == "");
        CT_REQUIRE(extension_of_pattern("img*.png") == "");
        CT_REQUIRE(extension_of_pattern("png") == "");
    }

    void test_desc()
    {
        using namespace catalyst::platform;

        // An empty description stays empty: no filters, nothing to derive an extension from.
        {
            const file_dialog_desc out = normalize_file_dialog_desc(file_dialog_desc{});
            CT_REQUIRE(out.filters.empty());
            CT_REQUIRE(out.default_filter == 0);
            CT_REQUIRE(out.default_extension.empty());
        }

        file_dialog_desc desc;
        desc.title = "Export";
        desc.default_name = "scene";
        desc.initial_folder = "assets";
        desc.filters = {{"Images", "png, jpg"}, {"", "tga"}, {"Anything", ""}};

        // The extension is derived from the default filter's first pattern when none is given.
        desc.default_filter = 0;
        {
            const file_dialog_desc out = normalize_file_dialog_desc(desc);
            CT_REQUIRE(out.filters.size() == 3);
            CT_REQUIRE(out.filters[0].name == "Images");
            CT_REQUIRE(out.filters[0].patterns == "*.png;*.jpg");
            CT_REQUIRE(out.filters[1].name == "*.tga"); // an unnamed filter is labelled with its patterns
            CT_REQUIRE(out.filters[1].patterns == "*.tga");
            CT_REQUIRE(out.filters[2].patterns == "*");
            CT_REQUIRE(out.default_extension == "png");

            // What the backend checks against the operating system is passed through untouched.
            CT_REQUIRE(out.title == "Export");
            CT_REQUIRE(out.default_name == "scene");
            CT_REQUIRE(out.initial_folder == desc.initial_folder);
        }

        desc.default_filter = 1;
        CT_REQUIRE(normalize_file_dialog_desc(desc).default_extension == "tga");

        // A filter that implies no single extension derives none.
        desc.default_filter = 2;
        CT_REQUIRE(normalize_file_dialog_desc(desc).default_extension.empty());

        // An out-of-range default falls back to the first filter.
        desc.default_filter = 17;
        {
            const file_dialog_desc out = normalize_file_dialog_desc(desc);
            CT_REQUIRE(out.default_filter == 0);
            CT_REQUIRE(out.default_extension == "png");
        }

        // An explicit extension wins over the derived one, and loses its dot.
        desc.default_filter = 0;
        desc.default_extension = ".jpeg";
        CT_REQUIRE(normalize_file_dialog_desc(desc).default_extension == "jpeg");

        // An unusable explicit extension falls back to the derived one rather than to nothing.
        desc.default_extension = "*.*";
        CT_REQUIRE(normalize_file_dialog_desc(desc).default_extension == "png");
    }

    void test_null_backend()
    {
        using namespace catalyst::platform;

        // Every error has a description, so "dialog: {}" always logs something.
        CT_REQUIRE(!name(dialog_error::cancelled).empty());
        CT_REQUIRE(!name(dialog_error::unsupported).empty());
        CT_REQUIRE(!name(dialog_error::failed).empty());

        window_desc wd;
        wd.visible = false;
        window w = create_window(wd);
        CT_REQUIRE(w);

        // Owned and unowned alike, the headless backend refuses at once rather than blocking.
        for (const window owner : {window{}, w})
        {
            file_dialog_desc desc;
            desc.owner = owner;
            desc.filters = {{"Images", "*.png"}};

            const auto open = open_file_dialog(desc);
            CT_REQUIRE(!open && open.error() == dialog_error::unsupported);

            const auto open_many = open_files_dialog(desc);
            CT_REQUIRE(!open_many && open_many.error() == dialog_error::unsupported);

            const auto save = save_file_dialog(desc);
            CT_REQUIRE(!save && save.error() == dialog_error::unsupported);

            const auto folder = pick_folder_dialog(desc);
            CT_REQUIRE(!folder && folder.error() == dialog_error::unsupported);

            message_box_desc box;
            box.owner = owner;
            box.title = "Catalyst";
            box.text = "Save changes?";
            box.buttons = message_box_buttons::yes_no_cancel;
            box.icon = message_box_icon::warning;

            const auto answer = show_message_box(box);
            CT_REQUIRE(!answer && answer.error() == dialog_error::unsupported);
        }

        destroy_window(w);
    }
} // namespace

int main()
{
    test_patterns();
    test_extensions();
    test_desc();
    test_null_backend();
    return 0;
}
