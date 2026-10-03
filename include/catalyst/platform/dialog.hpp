/**
 * @file dialog.hpp
 * @brief Native operating-system dialogs for the Catalyst platform library: open, save and folder pickers, and a
 * simple message box.
 * @details These are the dialogs the operating system draws itself, with its own look, its own places sidebar, its own
 * keyboard handling and its own accessibility support. They belong here rather than in catalyst::ui because only the
 * platform can show them, and catalyst::ui is deliberately kept free of any platform dependency; an in-engine file
 * browser drawn with the UI module is a separate, later thing.
 *
 * Every function in this header is synchronous: it shows the dialog, blocks the calling thread until the user dismisses
 * it, and returns the answer. While it blocks, the operating system runs a modal message loop of its own, and that loop
 * still dispatches the messages of every window on the thread. Window events therefore keep arriving on the bus
 * installed with set_event_bus, re-entrantly, from inside the dialog call -- the same situation as a border drag, and
 * served by the same remedy: install a frame callback with set_frame_callback if the window should keep drawing while
 * a dialog is up, because the application's own render loop is not running until the call returns.
 *
 * Call these from the thread that created the owner window and pumps its events. A dialog given an owner window is
 * modal to it: the owner is disabled for the duration, and it loses focus (so a window_focus_event arrives, held input
 * is released, and a captured cursor is let go until focus returns). Do not destroy the owner from an event handler
 * while its dialog is open.
 *
 * On Windows the file dialogs are COM objects. The caller need not initialise COM, and may already have done so in
 * either apartment: a thread in the multi-threaded one -- which the audio module puts the thread that opens a stream
 * in -- has the dialog shown from a helper thread while it keeps dispatching its own messages, so the behaviour above
 * is the same either way.
 *
 * Failures are values, as elsewhere in Catalyst: each function returns a std::expected whose error says whether the
 * user cancelled, the backend has no such dialog, or the operating system refused. Cancelling is the common case and is
 * reported as dialog_error::cancelled rather than as an empty result, so a caller cannot mistake it for a successful
 * pick of nothing.
 * License: MIT (see LICENSE).
 */

#pragma once

#include <catalyst/platform/window.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace catalyst::platform
{

    /**
     * @struct file_filter
     * @brief One entry in a file dialog's "file type" list: a label for the user and the patterns it matches.
     * @details @ref patterns is a list of wildcard patterns separated by ';' (',' and whitespace are accepted too), for
     * example "*.png;*.jpg". A token with no wildcard in it is taken as a bare extension, so "png", ".png" and "*.png"
     * all mean the same thing. An empty pattern list matches every file. An empty @ref name shows the patterns
     * themselves as the label.
     */
    struct file_filter
    {
        /** @brief The label shown in the file type list, UTF-8, e.g. "Images". */
        std::string name;

        /** @brief The patterns this entry matches, e.g. "*.png;*.jpg". See the struct description for the syntax. */
        std::string patterns;
    };

    /**
     * @struct file_dialog_desc
     * @brief Everything open_file_dialog, open_files_dialog, save_file_dialog and pick_folder_dialog can be told.
     * @details Every member has a default that leaves the choice to the operating system, so a default-constructed
     * description is a valid one. Members that do not apply to the dialog being shown are ignored: a folder picker has
     * no file types, and only a save dialog pre-fills a file name or appends an extension.
     */
    struct file_dialog_desc
    {
        /**
         * @var owner
         * @brief The window the dialog is modal to, or a default-constructed (invalid) window for an unowned dialog.
         * @details An owned dialog is placed over its owner and disables it until it closes. An owner that has already
         * been destroyed is treated as no owner at all.
         */
        window owner{};

        /** @brief The dialog's caption, UTF-8. Empty leaves the operating system's own ("Open", "Save As", ...). */
        std::string title;

        /**
         * @var initial_folder
         * @brief The folder the dialog opens in. Empty leaves the choice to the operating system, which usually
         * remembers the folder last used.
         * @details A relative path is resolved against the current working directory. A path to a file opens its
         * parent folder. A path that does not exist is ignored rather than reported.
         */
        std::filesystem::path initial_folder;

        /** @brief The file name a save dialog starts with, UTF-8, with or without an extension. Ignored by the others.
         */
        std::string default_name;

        /** @brief The file type list, in display order. Empty shows no file type list and accepts every file. */
        std::vector<file_filter> filters;

        /** @brief The index into @ref filters selected when the dialog opens. Out of range selects the first. */
        std::size_t default_filter = 0;

        /**
         * @var default_extension
         * @brief The extension a save dialog appends when the user types a name without one, without the dot (a
         * leading dot or "*." is tolerated and stripped).
         * @details Empty takes it from the selected filter's first pattern when that is a plain "*.ext" pattern, which
         * is what a user expects of a "PNG image (*.png)" entry; if it is not, nothing is appended. Once any default
         * extension is in effect the dialog also follows the file type the user selects, appending that type's
         * extension instead. Ignored by the other dialogs.
         */
        std::string default_extension;
    };

    /**
     * @enum dialog_error
     * @brief Why a dialog produced no answer.
     */
    enum class dialog_error : std::uint8_t
    {
        /** @brief The user dismissed the dialog without choosing: Cancel, Escape or the close button. */
        cancelled,

        /** @brief This platform backend has no native dialogs. The null backend returns this from everything. */
        unsupported,

        /** @brief The operating system could not show the dialog, or could not report what was chosen (a pick outside
         * the file system, such as a library or a phone, that has no path). */
        failed,
    };

    /**
     * @brief A short, stable description of an error. Never empty.
     * @details A lowercase sentence fragment that reads correctly after "dialog: ", so a caller can log
     * `"dialog: {}"` and get a whole message.
     */
    [[nodiscard]] constexpr std::string_view name(dialog_error error) noexcept
    {
        switch (error)
        {
        case dialog_error::cancelled:
            return "cancelled by the user";
        case dialog_error::unsupported:
            return "native dialogs are not supported by this platform backend";
        case dialog_error::failed:
            return "the operating system could not show the dialog";
        }
        return "unknown dialog error";
    }

    /**
     * @fn open_file_dialog
     * @brief Asks the user to choose one existing file. Blocks until the dialog is dismissed.
     * @param desc What to show; see file_dialog_desc.
     * @return The absolute path of the chosen file, or why there is none.
     */
    [[nodiscard]] std::expected<std::filesystem::path, dialog_error> open_file_dialog(const file_dialog_desc &desc);

    /**
     * @fn open_files_dialog
     * @brief Asks the user to choose one or more existing files. Blocks until the dialog is dismissed.
     * @param desc What to show; see file_dialog_desc.
     * @return The absolute paths of the chosen files, never empty, in the order the operating system reports them; or
     * why there are none.
     */
    [[nodiscard]] std::expected<std::vector<std::filesystem::path>, dialog_error>
    open_files_dialog(const file_dialog_desc &desc);

    /**
     * @fn save_file_dialog
     * @brief Asks the user to name a file to write. Blocks until the dialog is dismissed.
     * @details The file need not exist. If it does, the operating system asks the user to confirm overwriting it
     * before the dialog returns, so a path returned here is one the user agreed to replace. Nothing is created or
     * written: the caller does that.
     * @param desc What to show; see file_dialog_desc, in particular default_name and default_extension.
     * @return The absolute path the user chose, or why there is none.
     */
    [[nodiscard]] std::expected<std::filesystem::path, dialog_error> save_file_dialog(const file_dialog_desc &desc);

    /**
     * @fn pick_folder_dialog
     * @brief Asks the user to choose one existing folder. Blocks until the dialog is dismissed.
     * @param desc What to show; see file_dialog_desc. The filters, default_name and default_extension are ignored.
     * @return The absolute path of the chosen folder, or why there is none.
     */
    [[nodiscard]] std::expected<std::filesystem::path, dialog_error> pick_folder_dialog(const file_dialog_desc &desc);

    // -----------------------------------------------------------------------------
    // Message box
    // -----------------------------------------------------------------------------

    /**
     * @enum message_box_buttons
     * @brief Which buttons a message box offers.
     */
    enum class message_box_buttons : std::uint8_t
    {
        /** @brief A single OK. Closing the box any other way also answers ok. */
        ok,
        /** @brief OK and Cancel. Escape or the close button answers cancel. */
        ok_cancel,
        /** @brief Yes and No. There is no close button and Escape does nothing: the user must pick one. */
        yes_no,
        /** @brief Yes, No and Cancel, the "save changes before closing?" prompt. Escape or close answers cancel. */
        yes_no_cancel,
    };

    /**
     * @enum message_box_icon
     * @brief The icon a message box shows beside its text. The operating system may also play a matching sound.
     */
    enum class message_box_icon : std::uint8_t
    {
        none,
        info,
        warning,
        error,
    };

    /**
     * @enum message_box_result
     * @brief The button the user chose.
     */
    enum class message_box_result : std::uint8_t
    {
        ok,
        cancel,
        yes,
        no,
    };

    /**
     * @struct message_box_desc
     * @brief Everything show_message_box can be told.
     */
    struct message_box_desc
    {
        /** @brief The window the box is modal to, or an invalid window for none. See file_dialog_desc::owner. */
        window owner{};

        /** @brief The caption, UTF-8. Empty leaves the caption blank. */
        std::string title;

        /** @brief The message, UTF-8. Line breaks are honoured; long lines are wrapped by the operating system. */
        std::string text;

        /** @brief The buttons offered. */
        message_box_buttons buttons = message_box_buttons::ok;

        /** @brief The icon shown. */
        message_box_icon icon = message_box_icon::none;
    };

    /**
     * @fn show_message_box
     * @brief Shows a message and waits for the user to press one of its buttons. Blocks until the box is dismissed,
     * with the same modal-loop behaviour as the file dialogs (see the file description).
     * @details Pressing Cancel is an answer here, not an error: it comes back as message_box_result::cancel, and
     * dialog_error::cancelled is never returned. Each message_box_buttons value documents what closing the box
     * without pressing a button answers.
     * @param desc What to show.
     * @return The button the user chose, or dialog_error::unsupported / dialog_error::failed if the box could not be
     * shown at all.
     */
    [[nodiscard]] std::expected<message_box_result, dialog_error> show_message_box(const message_box_desc &desc);

} // namespace catalyst::platform
