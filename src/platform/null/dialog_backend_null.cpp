/**
 * @file dialog_backend_null.cpp
 * @brief The headless implementation of the native dialogs: there is no operating system to draw one, so every dialog
 * reports dialog_error::unsupported at once rather than blocking or inventing an answer. License: MIT (see LICENSE).
 */

#include "../detail_backend.hpp"

namespace catalyst::platform::detail
{

    std::expected<std::vector<std::filesystem::path>, dialog_error> show_file_dialog(file_dialog_kind /*kind*/,
                                                                                     const file_dialog_desc & /*desc*/)
    {
        return std::unexpected(dialog_error::unsupported);
    }

    std::expected<message_box_result, dialog_error> show_message_box(const message_box_desc & /*desc*/)
    {
        return std::unexpected(dialog_error::unsupported);
    }

} // namespace catalyst::platform::detail
