/**
 * @file offline_backend.cpp
 * @brief Deterministic, hardware-free audio backend. It honours the requested format exactly,
 * owns no threads, and advances only when the caller asks it to via `engine::render()`. That
 * makes the whole audio path -- format negotiation, callback dispatch, stream-time bookkeeping
 * and stats -- reproducible and testable in CI on any platform. Optionally retains rendered
 * output in memory and writes it as a 32-bit float WAV on shutdown.
 * License: CDDL-1.0 (see LICENSE).
 */

#include "../detail_backend.hpp"
#include "../detail_render.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

namespace catalyst::audio::detail
{

    namespace
    {

        void put_u32_le(std::vector<uint8_t> &out, uint32_t value)
        {
            out.push_back(static_cast<uint8_t>(value & 0xFF));
            out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
            out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
            out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        }

        void put_u16_le(std::vector<uint8_t> &out, uint16_t value)
        {
            out.push_back(static_cast<uint8_t>(value & 0xFF));
            out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        }

        void put_tag(std::vector<uint8_t> &out, const char (&tag)[5])
        {
            out.insert(out.end(), tag, tag + 4);
        }

        /// Writes interleaved float32 samples as a WAVE_FORMAT_IEEE_FLOAT `.wav`.
        ///
        /// Emits the 18-byte `fmt ` chunk and the `fact` chunk that the format tag requires,
        /// rather than the abbreviated 16-byte PCM header, so strict readers accept the file.
        bool write_float_wav(
            const std::string &path,
            std::span<const float> interleaved,
            uint32_t sample_rate,
            uint32_t channels)
        {
            if (path.empty() || channels == 0 || sample_rate == 0)
                return false;

            const uint64_t data_bytes = static_cast<uint64_t>(interleaved.size()) * sizeof(float);
            const uint64_t frames = channels ? (interleaved.size() / channels) : 0;

            // RIFF sizes are 32-bit; refuse rather than silently truncate.
            if (data_bytes + 64 > 0xFFFFFFFFull)
                return false;

            const uint32_t block_align = static_cast<uint32_t>(channels * sizeof(float));

            std::vector<uint8_t> header;
            header.reserve(64);

            put_tag(header, "RIFF");
            // "WAVE" + fmt chunk (8 + 18) + fact chunk (8 + 4) + data header (8) = 50
            put_u32_le(header, static_cast<uint32_t>(50 + data_bytes));
            put_tag(header, "WAVE");

            put_tag(header, "fmt ");
            put_u32_le(header, 18);
            put_u16_le(header, 3); // WAVE_FORMAT_IEEE_FLOAT
            put_u16_le(header, static_cast<uint16_t>(channels));
            put_u32_le(header, sample_rate);
            put_u32_le(header, sample_rate * block_align); // byte rate
            put_u16_le(header, static_cast<uint16_t>(block_align));
            put_u16_le(header, 32); // bits per sample
            put_u16_le(header, 0);  // cbSize

            put_tag(header, "fact");
            put_u32_le(header, 4);
            put_u32_le(header, static_cast<uint32_t>(frames));

            put_tag(header, "data");
            put_u32_le(header, static_cast<uint32_t>(data_bytes));

            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            if (!file)
                return false;

            file.write(reinterpret_cast<const char *>(header.data()), static_cast<std::streamsize>(header.size()));

            if (!interleaved.empty())
            {
                file.write(
                    reinterpret_cast<const char *>(interleaved.data()),
                    static_cast<std::streamsize>(data_bytes));
            }

            file.flush();
            return static_cast<bool>(file);
        }

        class offline_backend final : public backend
        {
        public:
            explicit offline_backend(engine_config config)
                : config_(std::move(config)), dispatcher_(config_.callback, config_.user, stats_) {}

            ~offline_backend() override { shutdown(); }

            std::string_view name() const noexcept override { return "Offline Renderer"; }
            engine_backend kind() const noexcept override { return engine_backend::offline; }

            std::expected<std::vector<device_info>, audio_error> enumerate_devices() const override
            {
                // A single synthetic endpoint, so device selection is exercisable without hardware.
                device_info device;
                device.id = "offline";
                device.name = "Offline Renderer";
                device.backend = engine_backend::offline;
                device.max_output_channels = 64;
                device.max_input_channels = 64;
                device.default_sample_rate = 48000;
                device.is_default = true;

                std::vector<device_info> devices;
                devices.push_back(std::move(device));
                return devices;
            }

            std::expected<void, audio_error> initialize() override
            {
                shutdown();

                sample_rate_ = config_.sample_rate;
                block_frames_ = config_.frames_per_buffer ? config_.frames_per_buffer : 512;

                const bool has_output =
                    config_.direction == stream_direction::output ||
                    config_.direction == stream_direction::duplex;
                const bool has_input =
                    config_.direction == stream_direction::input ||
                    config_.direction == stream_direction::duplex;

                output_channels_ = has_output ? config_.output_channels : 0;
                input_channels_ = has_input ? config_.input_channels : 0;

                if (output_channels_ == 0 && input_channels_ == 0)
                    return std::unexpected(audio_error::invalid_config);

                // The offline backend never renegotiates: reproducibility is the whole point.
                if (output_channels_ > 0)
                {
                    output_scratch_.assign(
                        static_cast<size_t>(block_frames_) * output_channels_, 0.0f);
                }

                if (input_channels_ > 0)
                {
                    input_scratch_.assign(
                        static_cast<size_t>(block_frames_) * input_channels_, 0.0f);
                }

                captured_.clear();
                if (config_.offline.capture_output && output_channels_ > 0 &&
                    config_.offline.max_capture_frames != 0)
                {
                    captured_.reserve(static_cast<size_t>(
                        config_.offline.max_capture_frames * output_channels_));
                }

                input_cursor_frames_ = 0;
                dispatcher_.reset_time();
                stats_.reset();

                initialized_ = true;
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
                const bool was_initialized = initialized_;

                stop();
                initialized_ = false;

                if (was_initialized && !config_.offline.wav_path.empty() &&
                    config_.offline.capture_output)
                {
                    // Best effort: `shutdown()` is noexcept and has no way to report failure.
                    // Callers that require the file must stat it themselves.
                    (void)write_float_wav(
                        config_.offline.wav_path,
                        std::span<const float>(captured_),
                        sample_rate_,
                        output_channels_);
                }

                output_scratch_.clear();
                output_scratch_.shrink_to_fit();
                input_scratch_.clear();
                input_scratch_.shrink_to_fit();
            }

            bool is_running() const noexcept override { return running_; }

            stream_info info() const override
            {
                stream_info out;
                out.backend = engine_backend::offline;
                out.direction = config_.direction;
                out.sample_rate = sample_rate_;
                out.output_channels = output_channels_;
                out.input_channels = input_channels_;
                out.buffer_frames = block_frames_;

                const double budget = sample_rate_
                                          ? static_cast<double>(block_frames_) / static_cast<double>(sample_rate_)
                                          : 0.0;
                out.output_latency_seconds = output_channels_ ? budget : 0.0;
                out.input_latency_seconds = input_channels_ ? budget : 0.0;

                out.exclusive = false;
                out.device_id = "offline";
                out.device_name = "Offline Renderer";
                return out;
            }

            stream_stats stats() const noexcept override { return stats_.snapshot(); }
            void reset_stats() noexcept override { stats_.reset(); }

            std::expected<uint64_t, audio_error> render(uint64_t frames) override
            {
                if (!initialized_)
                    return std::unexpected(audio_error::not_initialized);
                if (!running_)
                    return std::unexpected(audio_error::not_running);

                uint64_t remaining = frames;
                uint64_t rendered = 0;

                while (remaining > 0)
                {
                    const uint32_t block = static_cast<uint32_t>(
                        std::min<uint64_t>(remaining, block_frames_));

                    const float *input = nullptr;
                    if (input_channels_ > 0)
                    {
                        fill_input_block(block);
                        input = input_scratch_.data();
                    }

                    float *output = output_channels_ > 0 ? output_scratch_.data() : nullptr;

                    dispatcher_.dispatch(
                        output, input, block, output_channels_, input_channels_, sample_rate_);

                    if (output && config_.offline.capture_output)
                        append_capture(block);

                    remaining -= block;
                    rendered += block;
                }

                return rendered;
            }

            std::span<const float> captured_output() const noexcept override
            {
                return std::span<const float>(captured_);
            }

        private:
            /// Copies the next `frames` of caller-supplied capture data into the scratch block,
            /// zero-filling once the source is exhausted so the callback always sees a full block.
            void fill_input_block(uint32_t frames)
            {
                const size_t needed = static_cast<size_t>(frames) * input_channels_;
                std::fill_n(input_scratch_.begin(), needed, 0.0f);

                const auto &source = config_.offline;
                if (!source.input_frames || input_cursor_frames_ >= source.input_frame_count)
                {
                    input_cursor_frames_ += frames;
                    return;
                }

                const uint64_t available = source.input_frame_count - input_cursor_frames_;
                const uint64_t copy_frames = std::min<uint64_t>(available, frames);
                const size_t copy_samples = static_cast<size_t>(copy_frames) * input_channels_;

                const float *begin =
                    source.input_frames + (input_cursor_frames_ * input_channels_);

                std::copy_n(begin, copy_samples, input_scratch_.begin());
                input_cursor_frames_ += frames;
            }

            void append_capture(uint32_t frames)
            {
                const uint64_t cap = config_.offline.max_capture_frames;
                uint64_t storable = frames;

                if (cap != 0)
                {
                    const uint64_t held = captured_.size() / output_channels_;
                    if (held >= cap)
                        return;
                    storable = std::min<uint64_t>(frames, cap - held);
                }

                const size_t samples = static_cast<size_t>(storable) * output_channels_;
                captured_.insert(
                    captured_.end(), output_scratch_.begin(), output_scratch_.begin() + samples);
            }

            engine_config config_{};
            stats_block stats_;
            render_dispatcher dispatcher_;

            bool initialized_ = false;
            bool running_ = false;

            uint32_t sample_rate_ = 0;
            uint32_t block_frames_ = 0;
            uint32_t output_channels_ = 0;
            uint32_t input_channels_ = 0;

            std::vector<float> output_scratch_;
            std::vector<float> input_scratch_;
            std::vector<float> captured_;
            uint64_t input_cursor_frames_ = 0;
        };

    } // namespace

    std::unique_ptr<backend> create_offline_backend(const engine_config &config)
    {
        return std::make_unique<offline_backend>(config);
    }

} // namespace catalyst::audio::detail
