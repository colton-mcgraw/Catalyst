/**
 * @file detail_backend.hpp
 * @brief Internal interface every audio backend implements, plus the factory declarations the
 * backend registry dispatches to. Backends are selected at build time by CMake, which defines
 * the `CATALYST_AUDIO_HAS_*` macros; the offline and null backends are always present so the
 * module is buildable and testable on every platform.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include <catalyst/audio/engine.hpp>

#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace catalyst::audio::detail
{

    class backend
    {
    public:
        virtual ~backend() = default;

        virtual std::string_view name() const noexcept = 0;
        virtual engine_backend kind() const noexcept = 0;

        virtual std::expected<std::vector<device_info>, audio_error> enumerate_devices() const = 0;

        virtual std::expected<void, audio_error> initialize() = 0;
        virtual std::expected<void, audio_error> start() = 0;
        virtual void stop() noexcept = 0;
        virtual void shutdown() noexcept = 0;
        virtual bool is_running() const noexcept = 0;

        virtual stream_info info() const = 0;
        virtual stream_stats stats() const noexcept = 0;
        virtual void reset_stats() noexcept = 0;

        /// Advances a caller-clocked stream. Only the offline backend implements this; a live
        /// backend is clocked by its device, so driving it by hand is meaningless.
        virtual std::expected<uint64_t, audio_error> render(uint64_t frames)
        {
            (void)frames;
            return std::unexpected(audio_error::unsupported_operation);
        }

        /// Retained interleaved output, offline backend only.
        virtual std::span<const float> captured_output() const noexcept { return {}; }
    };

    /// Rejects configurations no backend can honour, so each backend does not re-check.
    std::expected<void, audio_error> validate_config(const engine_config &config) noexcept;

    /// Resolves `config.preferred_backend` (including `automatic`) and constructs the backend.
    /// The returned backend is constructed but not initialized.
    std::expected<std::unique_ptr<backend>, audio_error> create_backend(const engine_config &config);

    bool backend_available(engine_backend backend) noexcept;
    std::vector<engine_backend> available_backends();

    // Per-backend factories. Always available.
    std::unique_ptr<backend> create_offline_backend(const engine_config &config);
    std::unique_ptr<backend> create_null_backend(const engine_config &config);

#if defined(CATALYST_AUDIO_HAS_WASAPI)
    std::unique_ptr<backend> create_wasapi_backend_win32(const engine_config &config) noexcept;
#endif

#if defined(CATALYST_AUDIO_HAS_ASIO)
    std::unique_ptr<backend> create_asio_backend_win32(const engine_config &config) noexcept;
#endif

} // namespace catalyst::audio::detail
