/**
 * @file detail_win32.hpp
 * @brief Windows-only helpers shared by the WASAPI and ASIO backends: UTF-8/UTF-16 conversion and
 * the mapping from HRESULT to `audio_error`. Previously each backend carried its own copy of the
 * conversion routines.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#if defined(_WIN32)

#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif

#  include <windows.h>
#  include <audioclient.h>

#  include <catalyst/audio/engine.hpp>

#  include <string>
#  include <string_view>

namespace catalyst::audio::detail::win32
{

    inline std::string wide_to_utf8(std::wstring_view text)
    {
        if (text.empty())
            return {};

        const int needed = WideCharToMultiByte(
            CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

        if (needed <= 0)
            return {};

        std::string out(static_cast<size_t>(needed), '\0');

        const int written = WideCharToMultiByte(
            CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), needed, nullptr, nullptr);

        if (written <= 0)
            return {};

        out.resize(static_cast<size_t>(written));
        return out;
    }

    inline std::wstring utf8_to_wide(std::string_view text)
    {
        if (text.empty())
            return {};

        const int needed = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);

        if (needed <= 0)
            return {};

        std::wstring out(static_cast<size_t>(needed), L'\0');

        const int written = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), out.data(), needed);

        if (written <= 0)
            return {};

        out.resize(static_cast<size_t>(written));
        return out;
    }

    /// Translates the HRESULTs the audio APIs actually return into the module's error type, so
    /// callers can distinguish "someone else owns the device" from "the device vanished" without
    /// depending on Windows headers.
    inline audio_error error_from_hresult(HRESULT hr) noexcept
    {
        switch (hr)
        {
        case S_OK:
            return audio_error::none;
        case AUDCLNT_E_DEVICE_INVALIDATED:
            return audio_error::device_lost;
        case AUDCLNT_E_DEVICE_IN_USE:
        case AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED:
            return audio_error::device_busy;
        case AUDCLNT_E_UNSUPPORTED_FORMAT:
            return audio_error::format_unsupported;
        case AUDCLNT_E_ENDPOINT_CREATE_FAILED:
            return audio_error::no_device;
        case E_OUTOFMEMORY:
            return audio_error::platform_error;
        default:
            return audio_error::platform_error;
        }
    }

} // namespace catalyst::audio::detail::win32

#endif // _WIN32
