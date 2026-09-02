/**
 * @file backend_registry.cpp
 * @brief Resolves `engine_backend::automatic`, reports which backends this build contains, and
 * constructs the selected one. This is the single definition of `create_backend` in the module;
 * platform backends only expose their own factory, so adding or removing one from the build can
 * never produce duplicate or missing symbols.
 * License: CDDL-1.0 (see LICENSE).
 */

#include "detail_backend.hpp"

namespace catalyst::audio::detail
{

    namespace
    {

        // Guards against configurations that no device could satisfy, so backends do not each
        // re-derive the same sanity checks.
        constexpr uint32_t min_sample_rate = 4000;
        constexpr uint32_t max_sample_rate = 768000;
        constexpr uint32_t max_channels = 64;
        constexpr uint32_t max_frames_per_buffer = 1u << 20;

        bool wants_output(stream_direction direction) noexcept
        {
            return direction == stream_direction::output || direction == stream_direction::duplex;
        }

        bool wants_input(stream_direction direction) noexcept
        {
            return direction == stream_direction::input || direction == stream_direction::duplex;
        }

        engine_backend resolve(engine_backend requested) noexcept
        {
            if (requested != engine_backend::automatic)
                return requested;

            // `automatic` never resolves to `offline`: that backend has no clock of its own, so
            // selecting it implicitly would leave the caller with a stream that never runs.
#if defined(CATALYST_AUDIO_HAS_WASAPI)
            return engine_backend::wasapi;
#elif defined(CATALYST_AUDIO_HAS_ASIO)
            return engine_backend::asio;
#else
            return engine_backend::null;
#endif
        }

    } // namespace

    std::expected<void, audio_error> validate_config(const engine_config &config) noexcept
    {
        if (config.sample_rate < min_sample_rate || config.sample_rate > max_sample_rate)
            return std::unexpected(audio_error::invalid_config);

        if (config.frames_per_buffer > max_frames_per_buffer)
            return std::unexpected(audio_error::invalid_config);

        if (config.output_channels > max_channels || config.input_channels > max_channels)
            return std::unexpected(audio_error::invalid_config);

        if (wants_output(config.direction) && config.output_channels == 0)
            return std::unexpected(audio_error::invalid_config);

        if (wants_input(config.direction) && config.input_channels == 0)
            return std::unexpected(audio_error::invalid_config);

        // Writing a WAV requires the samples to have been retained.
        if (!config.offline.wav_path.empty() && !config.offline.capture_output)
            return std::unexpected(audio_error::invalid_config);

        return {};
    }

    bool backend_available(engine_backend backend) noexcept
    {
        switch (backend)
        {
        case engine_backend::automatic:
        case engine_backend::offline:
        case engine_backend::null:
            // Offline and null are compiled unconditionally, so `automatic` always resolves.
            return true;

        case engine_backend::wasapi:
#if defined(CATALYST_AUDIO_HAS_WASAPI)
            return true;
#else
            return false;
#endif

        case engine_backend::asio:
#if defined(CATALYST_AUDIO_HAS_ASIO)
            return true;
#else
            return false;
#endif

        case engine_backend::alsa:
        case engine_backend::coreaudio:
            return false;
        }

        return false;
    }

    std::vector<engine_backend> available_backends()
    {
        std::vector<engine_backend> out;
        out.reserve(4);

#if defined(CATALYST_AUDIO_HAS_WASAPI)
        out.push_back(engine_backend::wasapi);
#endif
#if defined(CATALYST_AUDIO_HAS_ASIO)
        out.push_back(engine_backend::asio);
#endif
        out.push_back(engine_backend::offline);
        out.push_back(engine_backend::null);

        return out;
    }

    std::expected<std::unique_ptr<backend>, audio_error> create_backend(const engine_config &config)
    {
        engine_config resolved = config;
        resolved.preferred_backend = resolve(config.preferred_backend);

        std::unique_ptr<backend> created;

        switch (resolved.preferred_backend)
        {
        case engine_backend::offline:
            created = create_offline_backend(resolved);
            break;

        case engine_backend::null:
            created = create_null_backend(resolved);
            break;

#if defined(CATALYST_AUDIO_HAS_WASAPI)
        case engine_backend::wasapi:
            created = create_wasapi_backend_win32(resolved);
            break;
#endif

#if defined(CATALYST_AUDIO_HAS_ASIO)
        case engine_backend::asio:
            created = create_asio_backend_win32(resolved);
            break;
#endif

        default:
            return std::unexpected(audio_error::backend_unavailable);
        }

        if (!created)
            return std::unexpected(audio_error::backend_unavailable);

        return created;
    }

} // namespace catalyst::audio::detail
