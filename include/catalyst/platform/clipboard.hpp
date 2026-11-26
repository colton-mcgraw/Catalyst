/**
 * @file clipboard.hpp
 * @brief The system clipboard, as text: reading it for a paste and writing it for a copy or cut.
 * @details The clipboard belongs to the operating system and is shared with every other program, so it lives here,
 * beside the windows and the native dialogs, rather than in catalyst::ui; a text field built with the UI module calls
 * these from its paste and copy commands.
 *
 * Only text is handled, as UTF-8, which is what Catalyst speaks everywhere else. Line breaks are normalised at the
 * boundary so an application only ever deals with one convention: text read from the clipboard has every "\r\n" and
 * lone "\r" turned into "\n", and text written to it is converted to the platform's own convention ("\r\n" on
 * Windows), so a multi-line copy pastes as multiple lines into other programs, including ones that predate bare "\n".
 *
 * The clipboard is not owned by the application, so every call can fail for reasons outside it: another program may
 * hold the clipboard open for a moment (reported as clipboard_error::busy, worth retrying on the next frame or
 * keypress rather than in a loop), or the clipboard may hold something other than text, such as an image or a list of
 * files (clipboard_error::no_text).
 *
 * Every function may be called from any thread and none of them needs a window: a backend that needs one to own what
 * it writes keeps a hidden one of its own. They are quick -- a copy of the text and no more -- but they are system
 * calls, so call them in response to a paste or copy command, not every frame.
 *
 * The null backend keeps an in-process clipboard, so copy and paste round-trip within one process (with the same line
 * break normalisation) and code that uses them can be tested headlessly.
 * License: MIT (see LICENSE).
 */

#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace catalyst::platform
{

    /**
     * @enum clipboard_error
     * @brief Why a clipboard call produced no text, or could not store it.
     */
    enum class clipboard_error : std::uint8_t
    {
        /** @brief The clipboard is empty, or holds something that is not text (an image, a list of files, ...). Only
         * get_clipboard_text reports this. An empty string that was copied as text is not this: it reads back as "". */
        no_text,

        /** @brief Another program has the clipboard open. This is transient -- usually a few milliseconds -- and the
         * backend has already retried briefly before reporting it, so a caller should give up on this command rather
         * than spin; the user can simply paste again. */
        busy,

        /** @brief The operating system refused for another reason, such as running out of memory for the copy. */
        failed,
    };

    /**
     * @brief A short, stable description of an error. Never empty.
     * @details A lowercase sentence fragment that reads correctly after "clipboard: ", so a caller can log
     * `"clipboard: {}"` and get a whole message.
     */
    [[nodiscard]] constexpr std::string_view name(clipboard_error error) noexcept
    {
        switch (error)
        {
        case clipboard_error::no_text:
            return "the clipboard holds no text";
        case clipboard_error::busy:
            return "another program has the clipboard open";
        case clipboard_error::failed:
            return "the operating system refused the clipboard operation";
        }
        return "unknown clipboard error";
    }

    /**
     * @fn get_clipboard_text
     * @brief Reads the text on the clipboard, for a paste.
     * @details The text is UTF-8 with line breaks normalised to "\n" (see the file description). It stops at the first
     * NUL, since that is where every platform's clipboard text ends; it may contain any other control character,
     * including tabs, so a single-line field that pastes it should decide what to do with those itself.
     * @return The text, possibly empty if an empty string was copied; or clipboard_error::no_text if there is no text
     * on the clipboard, clipboard_error::busy if another program holds it, clipboard_error::failed otherwise.
     */
    [[nodiscard]] std::expected<std::string, clipboard_error> get_clipboard_text();

    /**
     * @fn set_clipboard_text
     * @brief Replaces the clipboard's contents with @p utf8_text, for a copy or a cut.
     * @details Whatever was on the clipboard before, text or not, is gone afterwards. "\n" line breaks are converted to
     * the platform's convention (see the file description). The text stops at the first NUL, if it contains one. Text
     * that is not valid UTF-8 is read in the system's legacy code page instead, as window titles are, rather than
     * being refused. An empty string puts an empty text on the clipboard, which pastes as nothing.
     *
     * The text stays on the clipboard after the call returns, after the window that was focused is destroyed and after
     * the application exits: it has been handed to the operating system, not lent.
     * @param utf8_text The text to store.
     * @return Nothing on success; clipboard_error::busy if another program holds the clipboard,
     * clipboard_error::failed otherwise. Never clipboard_error::no_text. On failure the clipboard may already have
     * been emptied.
     */
    [[nodiscard]] std::expected<void, clipboard_error> set_clipboard_text(std::string_view utf8_text);

    /**
     * @fn has_clipboard_text
     * @brief Whether get_clipboard_text would find text, without reading it.
     * @details Cheaper than reading the text, and it does not need the clipboard to be free, so it suits deciding
     * whether a Paste menu item is enabled when the menu opens. The answer can be stale by the time the user picks the
     * item, since another program can change the clipboard at any moment: the paste itself must still handle
     * clipboard_error::no_text.
     * @return True if the clipboard holds text, including an empty text.
     */
    [[nodiscard]] bool has_clipboard_text() noexcept;

} // namespace catalyst::platform
