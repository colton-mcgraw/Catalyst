/**
 * @file engine.hpp
 * @brief Defines the audio engine class and related types for the Catalyst Audio module. The audio engine provides an interface for managing audio output and capture, including selecting audio backends, negotiating stream formats with the device, enumerating devices by stable identifier, and handling audio rendering through a user-provided real-time callback. This file also defines the engine configuration structure, the error type returned by every fallible operation, and the offline rendering options used for deterministic testing.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace catalyst::audio
{

    /// Selects which platform audio API the engine drives.
    enum class engine_backend : uint8_t
    {
        /// Pick the best backend available on this platform.
        automatic = 0,
        wasapi,
        asio,
        alsa,
        coreaudio,
        /// Deterministic, hardware-free backend that renders on demand. See `offline_options`.
        offline,
        /// Accepts every operation and produces nothing.
        null,
    };

    /// Failure reason for every fallible engine operation.
    enum class audio_error : uint8_t
    {
        none = 0,
        /// The operation requires a successfully initialized engine.
        not_initialized,
        /// The operation requires a started stream (e.g. `render()` before `start()`).
        not_running,
        /// `initialize()` was called on an engine that is already initialized.
        already_initialized,
        /// `engine_config` contains values the engine cannot honour (zero sample rate, no channels, ...).
        invalid_config,
        /// The requested backend was not compiled in, or is not present on this system.
        backend_unavailable,
        /// No device matched `engine_config::preferred_device`, and there is no usable default.
        no_device,
        /// The device disappeared or was invalidated while the stream was open.
        device_lost,
        /// The device cannot provide the requested format and fallback was disabled or exhausted.
        format_unsupported,
        /// The device is held exclusively by another process.
        device_busy,
        /// The render thread could not be created or joined.
        thread_failure,
        /// A file could not be opened or written (offline WAV capture).
        io_failure,
        /// The active backend does not implement this call (e.g. `render()` on a live backend).
        unsupported_operation,
        /// The platform API returned an unexpected failure.
        platform_error,
    };

    /// Stable, human-readable name for `error`. Never empty.
    std::string_view to_string(audio_error error) noexcept;

    /// Stable, human-readable name for `backend`. Never empty.
    std::string_view to_string(engine_backend backend) noexcept;

    /// Which half (or both) of the device the engine opens.
    enum class stream_direction : uint8_t
    {
        output = 0,
        input,
        duplex,
    };

    static_assert(sizeof(float) == 4, "float must be 32-bit IEEE 754");

    /// The one argument handed to `render_callback` for each block of audio.
    ///
    /// Samples are 32-bit float, interleaved, and nominally in [-1, 1]; backends clamp on
    /// conversion to fixed-point formats. float32 is used deliberately rather than double:
    /// every supported device format is float32 or narrower, so a wider pipeline would be
    /// truncated at the driver boundary while costing twice the memory bandwidth and half
    /// the SIMD width. Use `double` for phase accumulators, filter state and resampler
    /// positions inside your callback, and use `stream_time_frames` (never a float counter)
    /// for absolute time -- float32 stops representing consecutive integers at 2^24 frames,
    /// which is under six minutes at 48 kHz.
    struct render_context
    {
        /// Interleaved output, `frames * output_channels` samples. Null for input-only streams.
        /// Contents are undefined on entry; the callback must fill every sample.
        float *output = nullptr;

        /// Interleaved capture, `frames * input_channels` samples. Null unless the stream
        /// was opened for input or duplex.
        const float *input = nullptr;

        /// Frames in this block. Varies between callbacks; never assume `frames_per_buffer`.
        uint32_t frames = 0;

        uint32_t output_channels = 0;
        uint32_t input_channels = 0;

        /// The negotiated rate, which may differ from the rate you requested.
        uint32_t sample_rate = 0;

        /// Frames delivered on this stream before this block. Monotonic across the run,
        /// reset by `initialize()`. This is the correct clock for oscillators and envelopes.
        uint64_t stream_time_frames = 0;

        /// `engine_config::user`, verbatim.
        void *user = nullptr;
    };

    /// Fills one block of audio. Called on a real-time thread owned by the backend.
    ///
    /// This runs under a hard deadline. It must not allocate, take locks, perform I/O,
    /// block, or call into the engine. It is `noexcept` because there is no safe way to
    /// unwind out of a driver callback.
    using render_callback = void (*)(render_context &context) noexcept;

    /// What happened to the audio device.
    enum class device_change : uint8_t
    {
        /// The system default endpoint for this direction changed.
        default_device_changed,
        device_added,
        device_removed,
        /// The device backing the active stream went away; the stream is dead.
        device_lost,
    };

    /// Notifies the application that the device topology changed.
    ///
    /// Invoked on a platform-owned notification thread -- not the render thread and not the
    /// thread that called `initialize()`. Do not call back into the engine from here; post
    /// the event to your own queue (e.g. `catalyst::core::event_queue`) and act on it from a
    /// thread you control.
    using device_change_callback = void (*)(device_change change, std::string_view device_id, void *user) noexcept;

    /// One enumerated endpoint.
    struct device_info
    {
        /// Stable, backend-specific identifier. Pass this as `engine_config::preferred_device`.
        /// Prefer it over `name`: friendly names are not unique, and two identical headsets
        /// will collide.
        std::string id;

        /// Display name. Not unique, not stable across driver updates.
        std::string name;

        engine_backend backend = engine_backend::automatic;

        uint32_t max_output_channels = 0;
        uint32_t max_input_channels = 0;
        uint32_t default_sample_rate = 0;

        /// True if this is the system default endpoint for its direction.
        bool is_default = false;
    };

    /// The format the engine actually negotiated with the device.
    ///
    /// Always read this after `initialize()` succeeds. The device is free to refuse the
    /// requested rate, channel count or buffer size, and the engine will fall back rather
    /// than fail unless `engine_config::allow_format_fallback` is false.
    struct stream_info
    {
        engine_backend backend = engine_backend::automatic;
        stream_direction direction = stream_direction::output;

        uint32_t sample_rate = 0;
        uint32_t output_channels = 0;
        uint32_t input_channels = 0;

        /// Nominal frames per callback. Actual block sizes vary.
        uint32_t buffer_frames = 0;

        double output_latency_seconds = 0.0;
        double input_latency_seconds = 0.0;

        bool exclusive = false;

        std::string device_id;
        std::string device_name;
    };

    /// Health counters for the running stream. Sampled atomically; cheap to poll.
    struct stream_stats
    {
        uint64_t frames_rendered = 0;
        uint64_t callback_count = 0;

        /// Blocks the engine could not service in time, or device events that dropped audio.
        /// Any non-zero value here is audible.
        uint64_t xruns = 0;

        uint64_t device_changes = 0;

        double last_callback_seconds = 0.0;
        double peak_callback_seconds = 0.0;

        /// Peak callback duration as a fraction of the block's wall-clock budget.
        /// Sustained values above ~0.7 will glitch.
        double peak_load = 0.0;
    };

    /// Controls `engine_backend::offline`, which renders on demand with no hardware and no
    /// threads. `start()` does not spawn anything; call `engine::render()` to advance the
    /// stream by an exact number of frames. This makes the audio path fully deterministic
    /// and testable in CI.
    struct offline_options
    {
        /// Retain rendered output in memory, readable via `engine::captured_output()`.
        bool capture_output = true;

        /// Cap on retained frames; 0 means unbounded. Rendering continues past the cap,
        /// but the excess is not stored.
        uint64_t max_capture_frames = 0;

        /// When non-empty, `shutdown()` writes captured output as a 32-bit float WAV here.
        /// Requires `capture_output`.
        std::string wav_path;

        /// Optional interleaved capture data fed to `render_context::input` for input and
        /// duplex streams. Caller-owned; must outlive the engine. Reads past
        /// `input_frame_count` are zero-filled.
        const float *input_frames = nullptr;
        uint64_t input_frame_count = 0;
    };

    struct engine_config
    {
        engine_backend preferred_backend = engine_backend::automatic;

        /// `device_info::id` of the endpoint to open. A friendly name is also accepted as a
        /// fallback for convenience, but is ambiguous. Empty selects the system default.
        std::string preferred_device;

        stream_direction direction = stream_direction::output;

        uint32_t sample_rate = 48000;
        uint32_t output_channels = 2;
        uint32_t input_channels = 0;

        /// Requested block size. Treated as a hint; read `stream_info::buffer_frames` for
        /// what the device agreed to.
        uint32_t frames_per_buffer = 512;

        /// Take exclusive ownership of the device for lower latency. The device may refuse,
        /// or already be held by another process (`audio_error::device_busy`).
        bool exclusive = false;

        /// Accept a different rate/channel count/format if the device refuses the request.
        /// When false, a mismatch fails with `audio_error::format_unsupported`.
        bool allow_format_fallback = true;

        render_callback callback = nullptr;
        void *user = nullptr;

        device_change_callback on_device_change = nullptr;
        void *device_change_user = nullptr;

        offline_options offline;
    };

    class engine
    {
    public:
        engine();
        ~engine();

        engine(engine &&) noexcept;
        engine &operator=(engine &&) noexcept;

        engine(const engine &) = delete;
        engine &operator=(const engine &) = delete;

        /// Selects a backend, opens the device and negotiates a format.
        ///
        /// On success the stream is open but silent; call `start()`. Read `info()` afterwards
        /// to learn what was actually negotiated -- do not assume `config` was honoured.
        /// Fails with `already_initialized` rather than silently discarding `config`; call
        /// `shutdown()` first to reconfigure.
        std::expected<void, audio_error> initialize(const engine_config &config = {});

        /// Begins delivering callbacks. Idempotent while running.
        /// For `engine_backend::offline` this only arms the stream; use `render()` to advance it.
        std::expected<void, audio_error> start();

        /// Stops delivering callbacks and joins the render thread. Safe to call when stopped.
        void stop() noexcept;

        /// Stops, closes the device and releases the backend. Safe to call when uninitialized.
        void shutdown() noexcept;

        /// Synchronously renders exactly `frames` frames and returns the count rendered.
        ///
        /// Offline backend only; every live backend returns `unsupported_operation` because
        /// it is the device, not the caller, that owns the clock.
        std::expected<uint64_t, audio_error> render(uint64_t frames);

        /// Interleaved output retained by the offline backend. Empty for every other backend
        /// and when `offline_options::capture_output` is false. Invalidated by `render()`
        /// and `shutdown()`.
        std::span<const float> captured_output() const noexcept;

        /// Endpoints visible to this engine's backend. Returns owning values, so the result
        /// stays valid independently of the engine.
        std::expected<std::vector<device_info>, audio_error> devices() const;

        /// Endpoints visible to a specific backend, without constructing an engine.
        static std::expected<std::vector<device_info>, audio_error> devices(engine_backend backend);

        /// The negotiated format. Default-constructed until `initialize()` succeeds.
        stream_info info() const;

        stream_stats stats() const noexcept;
        void reset_stats() noexcept;

        bool is_initialized() const noexcept;
        bool is_running() const noexcept;

        /// The resolved backend, never `automatic` once initialized.
        engine_backend backend() const noexcept;

        /// Backend display name, e.g. "WASAPI". Empty before `initialize()`.
        std::string_view backend_name() const noexcept;

        /// True if `backend` was compiled in and can be constructed on this system.
        /// `automatic` is available whenever any backend is.
        static bool is_backend_available(engine_backend backend) noexcept;

        /// Every usable backend, best first. `automatic` is never listed.
        static std::vector<engine_backend> available_backends();

    private:
        struct impl;
        std::unique_ptr<impl> impl_;
    };

} // namespace catalyst::audio
