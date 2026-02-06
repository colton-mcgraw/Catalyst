#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <string_view>

namespace catalyst::audio
{

    enum class engine_backend : uint8_t
    {
        engine_backend_unknown = 0,
        engine_backend_asio,
        engine_backend_wasapi,
        engine_backend_alsa,
        engine_backend_coreaudio,
    };

    // Output callback signature.
    // - `output_interleaved`: interleaved float32 frames (channels * frames).
    // - The engine/backend owns the timing; you fill the buffer.
    static_assert(sizeof(float) == 4, "float must be 32-bit IEEE 754");
    using render_callback = void (*)(
        void *user,
        float *output_interleaved,
        uint32_t frames,
        uint32_t channels);

    struct engine_config
    {
        engine_backend preferred_backend = engine_backend::engine_backend_unknown;

        // Backend-specific preferred output device identifier.
        // - WASAPI: endpoint friendly name (as returned by enumerate_devices())
        // - ASIO: driver name (as returned by enumerate_devices())
        // Empty => default device.
        std::string preferred_device;

        uint32_t sample_rate = 48000;
        uint32_t channels = 2;
        uint32_t frames_per_buffer = 512;

        render_callback callback = nullptr;
        void *user = nullptr;
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

        // Returns false if the selected backend cannot initialize.
        bool initialize(const engine_config &config = {});
        std::vector<std::string_view> enumerate_devices() const;
        std::vector<std::string_view> enumerate_devices(engine_backend backend) const;
        bool start();
        void stop() noexcept;
        void shutdown() noexcept;

        bool is_initialized() const noexcept;
        bool is_running() const noexcept;
        std::string_view backend_name() const noexcept;

    private:
        struct impl;
        std::unique_ptr<impl> impl_;
    };

} // namespace catalyst::audio