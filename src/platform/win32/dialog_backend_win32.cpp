/**
 * @file dialog_backend_win32.cpp
 * @brief The Win32 implementation of the native dialogs: the Common Item Dialog (IFileOpenDialog / IFileSaveDialog)
 * for files and folders, and MessageBoxW for the message box.
 * @details The Common Item Dialog is a COM object, so each call enters a single-threaded apartment for its own duration
 * and leaves it again. A thread already in the multi-threaded apartment (CoInitializeEx reports RPC_E_CHANGED_MODE) is
 * left in it, since that apartment is not ours to leave, but the dialog is not shown from it: it is shown from a
 * helper thread instead while the caller pumps its messages, because showing it directly would freeze the caller's
 * windows and deadlock with an owner (see run_on_sta_thread). Every COM pointer is released before its apartment is
 * left.
 *
 * IFileDialog::Show and MessageBoxW both run a modal message loop that dispatches the messages of every window on the
 * thread, including the owner, whose window procedure (window_backend_win32.cpp) therefore keeps publishing events
 * and invoking its frame callback from inside the call. Nothing here has to arrange that; it is documented in
 * dialog.hpp so the application expects it.
 *
 * MessageBoxW is used rather than TaskDialogIndirect because the latter lives in comctl32 version 6, which a process
 * only gets through an application manifest that Catalyst cannot supply on the application's behalf.
 * License: MIT (see LICENSE).
 */

#include "../detail_backend.hpp"
#include "win32_helpers.hpp"

#include <win32/windows_lean.hpp>

// WIN32_LEAN_AND_MEAN leaves out the COM and shell headers, so they are included here, after <windows.h>.
#include <objbase.h>  // For CoInitializeEx, CoCreateInstance and CoTaskMemFree.
#include <shobjidl.h> // For IFileOpenDialog, IFileSaveDialog, IShellItem and SHCreateItemFromParsingName.

#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace catalyst::platform::detail
{
    /**
     * @namespace
     * @brief The COM plumbing the dialogs need, with internal linkage.
     */
    namespace
    {
        /**
         * @class com_ptr
         * @brief Owns one reference to a COM interface and releases it on destruction.
         * @details The audio backend has a fuller one; it is private to that module, and the dialogs need only this
         * much, so a dependency between the two backends would cost more than these few lines.
         */
        template <typename Interface>
        class com_ptr
        {
        public:
            com_ptr() noexcept = default;
            ~com_ptr() { reset(); }

            com_ptr(const com_ptr &) = delete;
            com_ptr &operator=(const com_ptr &) = delete;

            /** @brief The interface, still owned by this object. */
            [[nodiscard]] Interface *get() const noexcept { return pointer_; }
            [[nodiscard]] Interface *operator->() const noexcept { return pointer_; }
            [[nodiscard]] explicit operator bool() const noexcept { return pointer_ != nullptr; }

            /** @brief Releases the interface held, if any. */
            void reset() noexcept
            {
                if (Interface *p = std::exchange(pointer_, nullptr))
                    p->Release();
            }

            /** @brief Releases what is held and returns the slot for an out-parameter to fill. */
            [[nodiscard]] Interface **put() noexcept
            {
                reset();
                return &pointer_;
            }

        private:
            Interface *pointer_ = nullptr;
        };

        /**
         * @class com_scope
         * @brief Enters a single-threaded apartment for the lifetime of the object, if the thread is not in one yet.
         * @details Balanced on destruction only when this object's CoInitializeEx succeeded (S_OK, or S_FALSE when
         * the thread was already in a single-threaded apartment, which still takes a reference). RPC_E_CHANGED_MODE
         * means the thread is in the multi-threaded apartment -- the audio backends put the thread that opens a stream
         * there -- which is not ours to leave, and which is reported separately because the dialog must not be shown
         * from it directly (see show_file_dialog).
         */
        class com_scope
        {
        public:
            com_scope() noexcept : hr_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)) {}

            ~com_scope()
            {
                if (SUCCEEDED(hr_))
                    CoUninitialize();
            }

            com_scope(const com_scope &) = delete;
            com_scope &operator=(const com_scope &) = delete;

            /** @brief Whether the thread is now in a single-threaded apartment, entered here or before. */
            [[nodiscard]] bool single_threaded() const noexcept { return SUCCEEDED(hr_); }

            /** @brief Whether the thread was already in the multi-threaded apartment, which was left alone. */
            [[nodiscard]] bool multi_threaded() const noexcept { return hr_ == RPC_E_CHANGED_MODE; }

        private:
            HRESULT hr_;
        };

        /** @brief Frees a string the shell allocated with the COM task allocator. */
        struct co_task_free
        {
            void operator()(wchar_t *text) const noexcept { CoTaskMemFree(text); }
        };

        /** @brief What a failed HRESULT from IFileDialog::Show or a result query means to the caller. */
        [[nodiscard]] dialog_error error_from_hresult(HRESULT hr) noexcept
        {
            return hr == HRESULT_FROM_WIN32(ERROR_CANCELLED) ? dialog_error::cancelled : dialog_error::failed;
        }

        /** @brief The HWND behind an owner window, or nullptr for an invalid or destroyed one. */
        [[nodiscard]] HWND owner_hwnd(const window &owner) noexcept
        {
            if (!owner)
                return nullptr;

            const native_handle handle = get_native_handle(owner.id());
            if (handle.kind != native_handle_kind::win32_hwnd)
                return nullptr;
            return static_cast<HWND>(handle.handle);
        }

        /**
         * @fn file_system_path
         * @brief The file-system path of a shell item, or an empty path if it has none.
         * @details SIGDN_FILESYSPATH fails for items that are not in the file system -- a library root, a phone
         * attached over MTP. FOS_FORCEFILESYSTEM keeps the dialog from returning those, so failure here is not
         * expected, but it is still reported rather than assumed away.
         */
        [[nodiscard]] std::filesystem::path file_system_path(IShellItem *item)
        {
            wchar_t *raw = nullptr;
            if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw)) || raw == nullptr)
                return {};

            const std::unique_ptr<wchar_t, co_task_free> owned{raw};
            return std::filesystem::path{owned.get()};
        }

        /**
         * @fn set_initial_folder
         * @brief Points the dialog at file_dialog_desc::initial_folder, if it names a folder that exists.
         * @details IFileDialog::SetFolder is used rather than SetDefaultFolder, which is only a fallback for when the
         * shell remembers no recent folder of its own: an application that names a folder means it. The shell parses
         * only absolute paths with native separators, so the path is made into one first. Anything that goes wrong
         * leaves the operating system's choice in place, as documented for the field.
         */
        void set_initial_folder(IFileDialog *dialog, const std::filesystem::path &folder)
        {
            if (folder.empty())
                return;

            std::error_code ec;
            std::filesystem::path absolute = std::filesystem::absolute(folder, ec);
            if (ec)
                return;
            absolute = absolute.lexically_normal().make_preferred();

            if (std::filesystem::is_regular_file(absolute, ec))
                absolute = absolute.parent_path();
            if (!std::filesystem::is_directory(absolute, ec))
                return;

            com_ptr<IShellItem> item;
            if (SUCCEEDED(SHCreateItemFromParsingName(absolute.c_str(), nullptr, IID_PPV_ARGS(item.put()))))
                dialog->SetFolder(item.get());
        }

        /**
         * @fn set_file_types
         * @brief Installs the file type list and selects its default entry.
         * @details COMDLG_FILTERSPEC holds borrowed pointers, and IFileDialog::SetFileTypes copies the strings, so the
         * wide strings need to live only until the call returns. The type index is one-based.
         */
        [[nodiscard]] HRESULT set_file_types(IFileDialog *dialog, const file_dialog_desc &desc)
        {
            if (desc.filters.empty())
                return S_OK;

            std::vector<std::wstring> names;
            std::vector<std::wstring> specs;
            names.reserve(desc.filters.size());
            specs.reserve(desc.filters.size());
            for (const file_filter &filter : desc.filters)
            {
                names.push_back(win32::utf8_to_wide_or_ansi(filter.name));
                specs.push_back(win32::utf8_to_wide_or_ansi(filter.patterns));
            }

            std::vector<COMDLG_FILTERSPEC> entries(desc.filters.size());
            for (std::size_t i = 0; i < entries.size(); ++i)
                entries[i] = COMDLG_FILTERSPEC{names[i].c_str(), specs[i].c_str()};

            HRESULT hr = dialog->SetFileTypes(static_cast<UINT>(entries.size()), entries.data());
            if (FAILED(hr))
                return hr;

            return dialog->SetFileTypeIndex(static_cast<UINT>(desc.default_filter + 1));
        }

        /** @brief The FOS_ flags each kind of dialog adds to the dialog's own defaults. */
        [[nodiscard]] FILEOPENDIALOGOPTIONS options_for(file_dialog_kind kind) noexcept
        {
            // FOS_FORCEFILESYSTEM: the public API returns std::filesystem::path, so items outside the file system
            // cannot be chosen at all rather than being chosen and then failing. FOS_NOCHANGEDIR: an engine resolves
            // asset paths against its working directory, which a dialog must not move.
            FILEOPENDIALOGOPTIONS options = FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR;

            switch (kind)
            {
            case file_dialog_kind::open:
                options |= FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST;
                break;
            case file_dialog_kind::open_multiple:
                options |= FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_ALLOWMULTISELECT;
                break;
            case file_dialog_kind::save:
                options |= FOS_PATHMUSTEXIST | FOS_OVERWRITEPROMPT;
                break;
            case file_dialog_kind::pick_folder:
                options |= FOS_PICKFOLDERS | FOS_PATHMUSTEXIST;
                break;
            }

            return options;
        }

        /**
         * @fn configure
         * @brief Applies everything in the description that applies to this kind of dialog.
         * @details Only a failure to set the options is fatal: without FOS_PICKFOLDERS a folder picker would pick
         * files, and without FOS_FORCEFILESYSTEM the result might have no path. A title, a file name or a type list the
         * dialog refuses still leaves a usable dialog, so those failures are ignored and the defaults stand.
         */
        [[nodiscard]] HRESULT configure(IFileDialog *dialog, file_dialog_kind kind, const file_dialog_desc &desc)
        {
            FILEOPENDIALOGOPTIONS options = 0;
            HRESULT hr = dialog->GetOptions(&options);
            if (FAILED(hr))
                return hr;

            hr = dialog->SetOptions(options | options_for(kind));
            if (FAILED(hr))
                return hr;

            if (!desc.title.empty())
                (void)dialog->SetTitle(win32::utf8_to_wide_or_ansi(desc.title).c_str());

            set_initial_folder(dialog, desc.initial_folder);

            if (kind == file_dialog_kind::pick_folder)
                return S_OK;

            (void)set_file_types(dialog, desc);

            if (kind == file_dialog_kind::save)
            {
                if (!desc.default_extension.empty())
                    (void)dialog->SetDefaultExtension(win32::utf8_to_wide_or_ansi(desc.default_extension).c_str());
                if (!desc.default_name.empty())
                    (void)dialog->SetFileName(win32::utf8_to_wide_or_ansi(desc.default_name).c_str());
            }

            return S_OK;
        }

        /** @brief The paths of every item a multiple-selection open dialog returned. */
        [[nodiscard]] std::expected<std::vector<std::filesystem::path>, dialog_error>
        multiple_results(IFileDialog *dialog)
        {
            com_ptr<IFileOpenDialog> open;
            HRESULT hr = dialog->QueryInterface(IID_PPV_ARGS(open.put()));
            if (FAILED(hr))
                return std::unexpected(dialog_error::failed);

            com_ptr<IShellItemArray> items;
            hr = open->GetResults(items.put());
            if (FAILED(hr))
                return std::unexpected(error_from_hresult(hr));

            DWORD count = 0;
            if (FAILED(items->GetCount(&count)) || count == 0)
                return std::unexpected(dialog_error::failed);

            std::vector<std::filesystem::path> paths;
            paths.reserve(count);
            for (DWORD i = 0; i < count; ++i)
            {
                com_ptr<IShellItem> item;
                if (FAILED(items->GetItemAt(i, item.put())))
                    return std::unexpected(dialog_error::failed);

                std::filesystem::path path = file_system_path(item.get());
                if (path.empty())
                    return std::unexpected(dialog_error::failed);
                paths.push_back(std::move(path));
            }

            return paths;
        }

        /** @brief The path of the one item a single-selection dialog returned. */
        [[nodiscard]] std::expected<std::vector<std::filesystem::path>, dialog_error> single_result(IFileDialog *dialog)
        {
            com_ptr<IShellItem> item;
            const HRESULT hr = dialog->GetResult(item.put());
            if (FAILED(hr))
                return std::unexpected(error_from_hresult(hr));

            std::filesystem::path path = file_system_path(item.get());
            if (path.empty())
                return std::unexpected(dialog_error::failed);

            std::vector<std::filesystem::path> paths;
            paths.push_back(std::move(path));
            return paths;
        }
        /**
         * @fn run_file_dialog
         * @brief Creates, configures and shows the dialog, and reads back what was chosen.
         * @pre The calling thread is in a single-threaded apartment.
         * @param owner The owner's HWND, already looked up: the window registry belongs to the thread that pumps the
         * windows, and this may be running on another.
         */
        [[nodiscard]] std::expected<std::vector<std::filesystem::path>, dialog_error>
        run_file_dialog(file_dialog_kind kind, const file_dialog_desc &desc, HWND owner)
        {
            com_ptr<IFileDialog> dialog;
            const CLSID clsid = (kind == file_dialog_kind::save) ? __uuidof(FileSaveDialog) : __uuidof(FileOpenDialog);
            HRESULT hr = CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.put()));
            if (FAILED(hr))
                return std::unexpected(dialog_error::failed);

            hr = configure(dialog.get(), kind, desc);
            if (FAILED(hr))
                return std::unexpected(dialog_error::failed);

            hr = dialog->Show(owner);
            if (FAILED(hr))
                return std::unexpected(error_from_hresult(hr));

            return (kind == file_dialog_kind::open_multiple) ? multiple_results(dialog.get())
                                                             : single_result(dialog.get());
        }

        /** @brief Owns an event handle. */
        struct event_handle
        {
            HANDLE handle = CreateEventW(nullptr, TRUE, FALSE, nullptr);

            event_handle() noexcept = default;
            ~event_handle()
            {
                if (handle)
                    CloseHandle(handle);
            }

            event_handle(const event_handle &) = delete;
            event_handle &operator=(const event_handle &) = delete;
        };

        /**
         * @fn run_on_sta_thread
         * @brief run_file_dialog on a helper thread of its own, for a caller that is in the multi-threaded apartment,
         * with the calling thread pumping its messages until the helper finishes.
         * @details The Common Item Dialog is an apartment-threaded object. Created from the multi-threaded apartment,
         * COM puts it on a host thread of its own and blocks the caller in a call that dispatches no window messages.
         * That freezes every window the caller owns for as long as the dialog is open, and with an owner it deadlocks
         * outright: the dialog disables its owner and attaches to its input queue with messages sent to the owner's
         * thread, which is blocked waiting for the dialog. (The audio backends join the multi-threaded apartment on the
         * thread that opens a stream, so this is an ordinary Catalyst configuration, not a contrived one.)
         *
         * So the dialog gets a thread in a single-threaded apartment of its own, and the caller waits for it with
         * MsgWaitForMultipleObjectsEx, dispatching what arrives in the meantime -- which is what the dialog's own modal
         * loop would have done on the caller's thread, so window events and frame callbacks keep coming exactly as
         * dialog.hpp documents. A WM_QUIT seen while waiting is held and re-posted once the dialog is gone, the usual
         * courtesy of a modal loop that is not the application's own.
         */
        [[nodiscard]] std::expected<std::vector<std::filesystem::path>, dialog_error>
        run_on_sta_thread(file_dialog_kind kind, const file_dialog_desc &desc, HWND owner)
        {
            const event_handle done;
            if (!done.handle)
                return std::unexpected(dialog_error::failed);

            std::expected<std::vector<std::filesystem::path>, dialog_error> result =
                std::unexpected(dialog_error::failed);

            std::thread helper;
            try
            {
                helper = std::thread(
                    [&]() noexcept
                    {
                        try
                        {
                            const com_scope com;
                            if (com.single_threaded())
                                result = run_file_dialog(kind, desc, owner);
                        }
                        catch (...)
                        {
                            // Out of memory converting a string or building a path: reported as a failure below.
                        }
                        SetEvent(done.handle);
                    });
            }
            catch (const std::system_error &)
            {
                return std::unexpected(dialog_error::failed);
            }

            bool quit = false;
            WPARAM quit_code = 0;
            for (;;)
            {
                const DWORD wait =
                    MsgWaitForMultipleObjectsEx(1, &done.handle, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                if (wait == WAIT_OBJECT_0)
                    break;

                if (wait != WAIT_OBJECT_0 + 1)
                {
                    // The wait itself failed. The helper still has to be joined, so wait for it without pumping.
                    WaitForSingleObject(done.handle, INFINITE);
                    break;
                }

                MSG msg{};
                while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
                {
                    if (msg.message == WM_QUIT)
                    {
                        quit = true;
                        quit_code = msg.wParam;
                        continue;
                    }
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
            }

            helper.join();

            if (quit)
                PostQuitMessage(static_cast<int>(quit_code));

            return result;
        }
    } // namespace

    std::expected<std::vector<std::filesystem::path>, dialog_error> show_file_dialog(file_dialog_kind kind,
                                                                                     const file_dialog_desc &desc)
    {
        // A dialog can be the first thing a tool application shows, before any window; DPI awareness has to be
        // declared before then or the dialog is bitmap-stretched on a scaled display.
        win32::ensure_process_dpi_awareness();

        const HWND owner = owner_hwnd(desc.owner);

        const com_scope com;
        if (com.single_threaded())
            return run_file_dialog(kind, desc, owner);
        if (com.multi_threaded())
            return run_on_sta_thread(kind, desc, owner);
        return std::unexpected(dialog_error::failed);
    }

    std::expected<message_box_result, dialog_error> show_message_box(const message_box_desc &desc)
    {
        win32::ensure_process_dpi_awareness();

        UINT flags = 0;
        switch (desc.buttons)
        {
        case message_box_buttons::ok:
            flags |= MB_OK;
            break;
        case message_box_buttons::ok_cancel:
            flags |= MB_OKCANCEL;
            break;
        case message_box_buttons::yes_no:
            flags |= MB_YESNO;
            break;
        case message_box_buttons::yes_no_cancel:
            flags |= MB_YESNOCANCEL;
            break;
        }

        switch (desc.icon)
        {
        case message_box_icon::none:
            break;
        case message_box_icon::info:
            flags |= MB_ICONINFORMATION;
            break;
        case message_box_icon::warning:
            flags |= MB_ICONWARNING;
            break;
        case message_box_icon::error:
            flags |= MB_ICONERROR;
            break;
        }

        // A null caption would make Windows title the box "Error" whatever the icon says, so an empty title is passed
        // as an empty string, which leaves the caption blank as documented.
        const std::wstring title = win32::utf8_to_wide_or_ansi(desc.title);
        const std::wstring text = win32::utf8_to_wide_or_ansi(desc.text);

        switch (MessageBoxW(owner_hwnd(desc.owner), text.c_str(), title.c_str(), flags))
        {
        case IDOK:
            return message_box_result::ok;
        case IDCANCEL:
            return message_box_result::cancel;
        case IDYES:
            return message_box_result::yes;
        case IDNO:
            return message_box_result::no;
        default:
            // Zero is a failure to show the box at all; nothing else can come back from these button sets.
            return std::unexpected(dialog_error::failed);
        }
    }

} // namespace catalyst::platform::detail
