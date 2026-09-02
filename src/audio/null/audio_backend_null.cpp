/**
 * @file audio_backend_null.cpp
 * @brief Backend that accepts every operation and produces no audio. Used when the platform has
 * no supported audio API, or when an application wants the audio module present but silent. It
 * spawns no threads and never invokes the render callback; for a silent-but-driven stream that
 * still exercises the callback, use `engine_backend::offline` instead.
 * License: CDDL-1.0 (see LICENSE).
 */

#include "../detail_backend.hpp"
#include "../detail_render.hpp"

namespace catalyst::audio::detail
{

    namespace
    {

        class null_backend final : public backend
        {
        public:
            explicit null_backend(engine_config config) : config_(std::move(config)) {}

            std::string_view name() const noexcept override { return "Null"; }
            engine_backend kind() const noexcept override { return engine_backend::null; }

            std::expected<std::vector<device_info>, audio_error> enumerate_devices() const override
            {
                return std::vector<device_info>{};
            }

            std::expected<void, audio_error> initialize() override
            {
                initialized_ = true;
                stats_.reset();
                return {};
            }

            std::expected<void, audio_error> start() override
            {
                if (!initialized_)
                    return std::unexpected(audio_error::not_initialized);

                running_ = true;
                return {};
            }

            void stop() noexcept override { running_ = false; }

            void shutdown() noexcept override
            {
                running_ = false;
                initialized_ = false;
            }

            bool is_running() const noexcept override { return running_; }

            stream_info info() const override
            {
                stream_info out;
                out.backend = engine_backend::null;
                out.direction = config_.direction;
                out.sample_rate = config_.sample_rate;
                out.output_channels = config_.output_channels;
                out.input_channels = config_.input_channels;
                out.buffer_frames = config_.frames_per_buffer;
                out.device_id = "null";
                out.device_name = "Null";
                return out;
            }

            stream_stats stats() const noexcept override { return stats_.snapshot(); }
            void reset_stats() noexcept override { stats_.reset(); }

        private:
            engine_config config_{};
            stats_block stats_;
            bool initialized_ = false;
            bool running_ = false;
        };

    } // namespace

    std::unique_ptr<backend> create_null_backend(const engine_config &config)
    {
        return std::make_unique<null_backend>(config);
    }

} // namespace catalyst::audio::detail
