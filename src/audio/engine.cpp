/**
 * @file engine.cpp
 * @brief Implements the audio engine facade: lifecycle management, backend selection and the
 * pass-through to the active backend. All device and format specifics live in the backends;
 * this layer only enforces the state machine and converts backend results into `audio_error`.
 * License: CDDL-1.0 (see LICENSE).
 */

#include <catalyst/audio/engine.hpp>

#include "detail_backend.hpp"

#include <utility>

namespace catalyst::audio
{

    std::string_view to_string(audio_error error) noexcept
    {
        switch (error)
        {
        case audio_error::none:
            return "none";
        case audio_error::not_initialized:
            return "engine is not initialized";
        case audio_error::not_running:
            return "audio stream is not running";
        case audio_error::already_initialized:
            return "engine is already initialized";
        case audio_error::invalid_config:
            return "engine configuration is invalid";
        case audio_error::backend_unavailable:
            return "requested audio backend is unavailable";
        case audio_error::no_device:
            return "no matching audio device";
        case audio_error::device_lost:
            return "audio device was lost";
        case audio_error::format_unsupported:
            return "audio device cannot provide the requested format";
        case audio_error::device_busy:
            return "audio device is in use by another process";
        case audio_error::thread_failure:
            return "audio render thread failure";
        case audio_error::io_failure:
            return "audio file I/O failure";
        case audio_error::unsupported_operation:
            return "operation is not supported by this backend";
        case audio_error::platform_error:
            return "platform audio API failure";
        }

        return "unknown audio error";
    }

    std::string_view to_string(engine_backend backend) noexcept
    {
        switch (backend)
        {
        case engine_backend::automatic:
            return "automatic";
        case engine_backend::wasapi:
            return "WASAPI";
        case engine_backend::asio:
            return "ASIO";
        case engine_backend::alsa:
            return "ALSA";
        case engine_backend::coreaudio:
            return "CoreAudio";
        case engine_backend::offline:
            return "offline";
        case engine_backend::null:
            return "null";
        }

        return "unknown";
    }

    struct engine::impl
    {
        engine_config config{};
        std::unique_ptr<detail::backend> backend;
        bool initialized = false;
        bool running = false;
    };

    engine::engine() : impl_(std::make_unique<impl>()) {}

    engine::~engine()
    {
        shutdown();
    }

    engine::engine(engine &&other) noexcept = default;
    engine &engine::operator=(engine &&other) noexcept = default;

    std::expected<void, audio_error> engine::initialize(const engine_config &config)
    {
        if (!impl_)
            return std::unexpected(audio_error::not_initialized);

        // Refuse rather than silently discard the caller's new configuration.
        if (impl_->initialized)
            return std::unexpected(audio_error::already_initialized);

        if (const auto valid = detail::validate_config(config); !valid)
            return std::unexpected(valid.error());

        auto created = detail::create_backend(config);
        if (!created)
            return std::unexpected(created.error());

        impl_->config = config;
        impl_->backend = std::move(*created);

        if (const auto opened = impl_->backend->initialize(); !opened)
        {
            // Leave no half-open device behind on failure.
            impl_->backend->shutdown();
            impl_->backend.reset();
            return std::unexpected(opened.error());
        }

        impl_->initialized = true;
        impl_->running = false;
        return {};
    }

    std::expected<void, audio_error> engine::start()
    {
        if (!impl_ || !impl_->initialized || !impl_->backend)
            return std::unexpected(audio_error::not_initialized);

        if (impl_->running)
            return {};

        if (const auto started = impl_->backend->start(); !started)
            return std::unexpected(started.error());

        impl_->running = true;
        return {};
    }

    void engine::stop() noexcept
    {
        if (!impl_ || !impl_->backend || !impl_->running)
            return;

        impl_->backend->stop();
        impl_->running = false;
    }

    void engine::shutdown() noexcept
    {
        if (!impl_)
            return;

        if (impl_->backend)
        {
            stop();
            impl_->backend->shutdown();
            impl_->backend.reset();
        }

        impl_->initialized = false;
        impl_->running = false;
    }

    std::expected<uint64_t, audio_error> engine::render(uint64_t frames)
    {
        if (!impl_ || !impl_->initialized || !impl_->backend)
            return std::unexpected(audio_error::not_initialized);

        return impl_->backend->render(frames);
    }

    std::span<const float> engine::captured_output() const noexcept
    {
        if (!impl_ || !impl_->backend)
            return {};

        return impl_->backend->captured_output();
    }

    std::expected<std::vector<device_info>, audio_error> engine::devices() const
    {
        if (!impl_)
            return std::unexpected(audio_error::not_initialized);

        if (impl_->backend)
            return impl_->backend->enumerate_devices();

        // Not initialized yet: build a throwaway backend just to enumerate.
        auto temporary = detail::create_backend(impl_->config);
        if (!temporary)
            return std::unexpected(temporary.error());

        return (*temporary)->enumerate_devices();
    }

    std::expected<std::vector<device_info>, audio_error> engine::devices(engine_backend backend)
    {
        engine_config config;
        config.preferred_backend = backend;

        auto temporary = detail::create_backend(config);
        if (!temporary)
            return std::unexpected(temporary.error());

        return (*temporary)->enumerate_devices();
    }

    stream_info engine::info() const
    {
        if (!impl_ || !impl_->backend || !impl_->initialized)
            return {};

        return impl_->backend->info();
    }

    stream_stats engine::stats() const noexcept
    {
        if (!impl_ || !impl_->backend)
            return {};

        return impl_->backend->stats();
    }

    void engine::reset_stats() noexcept
    {
        if (!impl_ || !impl_->backend)
            return;

        impl_->backend->reset_stats();
    }

    bool engine::is_initialized() const noexcept
    {
        return impl_ && impl_->initialized;
    }

    bool engine::is_running() const noexcept
    {
        return impl_ && impl_->running;
    }

    engine_backend engine::backend() const noexcept
    {
        if (!impl_ || !impl_->backend)
            return engine_backend::automatic;

        return impl_->backend->kind();
    }

    std::string_view engine::backend_name() const noexcept
    {
        if (!impl_ || !impl_->backend)
            return {};

        return impl_->backend->name();
    }

    bool engine::is_backend_available(engine_backend backend) noexcept
    {
        return detail::backend_available(backend);
    }

    std::vector<engine_backend> engine::available_backends()
    {
        return detail::available_backends();
    }

} // namespace catalyst::audio
