/**
 * @file asio_backend_win32.cpp
 * @brief ASIO output/input/duplex backend for Windows, driven through the SDK-free loader in
 * `asio_loader.h`. Drivers are identified by their CLSID rather than their display name, the
 * negotiated sample rate and buffer size are read back from the driver rather than assumed, and
 * sample-format conversion is resolved once at initialization instead of being re-tested for
 * every channel of every block.
 * License: CDDL-1.0 (see LICENSE).
 */

#include "../detail_backend.hpp"

#if defined(_WIN32)

#include "../detail_render.hpp"
#include "../win32/detail_win32.hpp"

#include "asio_loader.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <objbase.h>

namespace catalyst::audio::detail
{

    namespace
    {
        using win32::utf8_to_wide;
        using win32::wide_to_utf8;

        std::string guid_to_string(const GUID &guid)
        {
            static constexpr char digits[] = "0123456789ABCDEF";

            std::string out;
            out.reserve(38);
            out.push_back('{');

            const auto push_byte = [&out](uint8_t value) {
                out.push_back(digits[(value >> 4) & 0x0F]);
                out.push_back(digits[value & 0x0F]);
            };

            const auto push_u32 = [&](uint32_t value) {
                push_byte(static_cast<uint8_t>((value >> 24) & 0xFF));
                push_byte(static_cast<uint8_t>((value >> 16) & 0xFF));
                push_byte(static_cast<uint8_t>((value >> 8) & 0xFF));
                push_byte(static_cast<uint8_t>(value & 0xFF));
            };

            const auto push_u16 = [&](uint16_t value) {
                push_byte(static_cast<uint8_t>((value >> 8) & 0xFF));
                push_byte(static_cast<uint8_t>(value & 0xFF));
            };

            push_u32(guid.Data1);
            out.push_back('-');
            push_u16(guid.Data2);
            out.push_back('-');
            push_u16(guid.Data3);
            out.push_back('-');
            push_byte(guid.Data4[0]);
            push_byte(guid.Data4[1]);
            out.push_back('-');
            for (int i = 2; i < 8; ++i)
                push_byte(guid.Data4[static_cast<size_t>(i)]);
            out.push_back('}');

            return out;
        }

        float clamp_unit(float value) noexcept
        {
            if (value < -1.0f)
                return -1.0f;
            if (value > 1.0f)
                return 1.0f;
            return value;
        }

        // ---------------------------------------------------------------------------------
        // Sample stores. Each knows its width and how to move one sample to or from float,
        // byteswapping in the same pass rather than making a second sweep over the block.
        // ---------------------------------------------------------------------------------

        struct store_f32
        {
            static constexpr size_t width = 4;

            static void write(uint8_t *dst, float value, bool swap) noexcept
            {
                uint32_t bits = std::bit_cast<uint32_t>(value);
                if (swap)
                    bits = std::byteswap(bits);
                std::memcpy(dst, &bits, sizeof(bits));
            }

            static float read(const uint8_t *src, bool swap) noexcept
            {
                uint32_t bits = 0;
                std::memcpy(&bits, src, sizeof(bits));
                if (swap)
                    bits = std::byteswap(bits);
                return std::bit_cast<float>(bits);
            }
        };

        struct store_f64
        {
            static constexpr size_t width = 8;

            static void write(uint8_t *dst, float value, bool swap) noexcept
            {
                uint64_t bits = std::bit_cast<uint64_t>(static_cast<double>(value));
                if (swap)
                    bits = std::byteswap(bits);
                std::memcpy(dst, &bits, sizeof(bits));
            }

            static float read(const uint8_t *src, bool swap) noexcept
            {
                uint64_t bits = 0;
                std::memcpy(&bits, src, sizeof(bits));
                if (swap)
                    bits = std::byteswap(bits);
                return static_cast<float>(std::bit_cast<double>(bits));
            }
        };

        struct store_i16
        {
            static constexpr size_t width = 2;

            static void write(uint8_t *dst, float value, bool swap) noexcept
            {
                const auto sample = static_cast<int16_t>(
                    std::lround(static_cast<double>(clamp_unit(value)) * 32767.0));
                uint16_t bits = static_cast<uint16_t>(sample);
                if (swap)
                    bits = std::byteswap(bits);
                std::memcpy(dst, &bits, sizeof(bits));
            }

            static float read(const uint8_t *src, bool swap) noexcept
            {
                uint16_t bits = 0;
                std::memcpy(&bits, src, sizeof(bits));
                if (swap)
                    bits = std::byteswap(bits);
                return static_cast<float>(static_cast<int16_t>(bits)) * (1.0f / 32768.0f);
            }
        };

        struct store_i32
        {
            static constexpr size_t width = 4;

            static void write(uint8_t *dst, float value, bool swap) noexcept
            {
                const auto sample = static_cast<int32_t>(
                    std::llround(static_cast<double>(clamp_unit(value)) * 2147483647.0));
                uint32_t bits = static_cast<uint32_t>(sample);
                if (swap)
                    bits = std::byteswap(bits);
                std::memcpy(dst, &bits, sizeof(bits));
            }

            static float read(const uint8_t *src, bool swap) noexcept
            {
                uint32_t bits = 0;
                std::memcpy(&bits, src, sizeof(bits));
                if (swap)
                    bits = std::byteswap(bits);
                return static_cast<float>(static_cast<int32_t>(bits)) * (1.0f / 2147483648.0f);
            }
        };

        struct store_i24
        {
            static constexpr size_t width = 3;

            static void write(uint8_t *dst, float value, bool swap) noexcept
            {
                const auto sample = static_cast<int32_t>(
                    std::llround(static_cast<double>(clamp_unit(value)) * 8388607.0));

                uint8_t bytes[3] = {
                    static_cast<uint8_t>(sample & 0xFF),
                    static_cast<uint8_t>((sample >> 8) & 0xFF),
                    static_cast<uint8_t>((sample >> 16) & 0xFF),
                };

                if (swap)
                    std::swap(bytes[0], bytes[2]);

                std::memcpy(dst, bytes, sizeof(bytes));
            }

            static float read(const uint8_t *src, bool swap) noexcept
            {
                uint8_t bytes[3];
                std::memcpy(bytes, src, sizeof(bytes));
                if (swap)
                    std::swap(bytes[0], bytes[2]);

                int32_t sample = static_cast<int32_t>(bytes[0]) |
                                 (static_cast<int32_t>(bytes[1]) << 8) |
                                 (static_cast<int32_t>(bytes[2]) << 16);

                // Sign-extend from 24 bits.
                if (sample & 0x00800000)
                    sample |= static_cast<int32_t>(0xFF000000u);

                return static_cast<float>(sample) * (1.0f / 8388608.0f);
            }
        };

        using deinterleave_fn = void (*)(uint8_t *, const float *, int32_t, int32_t) noexcept;
        using interleave_fn = void (*)(float *, const uint8_t *, int32_t, int32_t) noexcept;

        template <typename Store, bool Swap>
        void deinterleave_block(uint8_t *dst, const float *src, int32_t frames, int32_t stride) noexcept
        {
            for (int32_t frame = 0; frame < frames; ++frame)
            {
                Store::write(
                    dst + static_cast<size_t>(frame) * Store::width,
                    src[static_cast<size_t>(frame) * static_cast<size_t>(stride)],
                    Swap);
            }
        }

        template <typename Store, bool Swap>
        void interleave_block(float *dst, const uint8_t *src, int32_t frames, int32_t stride) noexcept
        {
            for (int32_t frame = 0; frame < frames; ++frame)
            {
                dst[static_cast<size_t>(frame) * static_cast<size_t>(stride)] =
                    Store::read(src + static_cast<size_t>(frame) * Store::width, Swap);
            }
        }

        /// Resolves the conversion for a channel once, at initialization, so the real-time path
        /// is an indirect call rather than a chain of type comparisons per channel per block.
        deinterleave_fn pick_deinterleave(asio::asio_sample_type type) noexcept
        {
            using t = asio::asio_sample_type;
            switch (type)
            {
            case t::float32_lsb:
                return &deinterleave_block<store_f32, false>;
            case t::float32_msb:
                return &deinterleave_block<store_f32, true>;
            case t::float64_lsb:
                return &deinterleave_block<store_f64, false>;
            case t::float64_msb:
                return &deinterleave_block<store_f64, true>;
            case t::int16_lsb:
                return &deinterleave_block<store_i16, false>;
            case t::int16_msb:
                return &deinterleave_block<store_i16, true>;
            case t::int24_lsb:
                return &deinterleave_block<store_i24, false>;
            case t::int24_msb:
                return &deinterleave_block<store_i24, true>;
            case t::int32_lsb:
                return &deinterleave_block<store_i32, false>;
            case t::int32_msb:
                return &deinterleave_block<store_i32, true>;
            default:
                return nullptr;
            }
        }

        interleave_fn pick_interleave(asio::asio_sample_type type) noexcept
        {
            using t = asio::asio_sample_type;
            switch (type)
            {
            case t::float32_lsb:
                return &interleave_block<store_f32, false>;
            case t::float32_msb:
                return &interleave_block<store_f32, true>;
            case t::float64_lsb:
                return &interleave_block<store_f64, false>;
            case t::float64_msb:
                return &interleave_block<store_f64, true>;
            case t::int16_lsb:
                return &interleave_block<store_i16, false>;
            case t::int16_msb:
                return &interleave_block<store_i16, true>;
            case t::int24_lsb:
                return &interleave_block<store_i24, false>;
            case t::int24_msb:
                return &interleave_block<store_i24, true>;
            case t::int32_lsb:
                return &interleave_block<store_i32, false>;
            case t::int32_msb:
                return &interleave_block<store_i32, true>;
            default:
                return nullptr;
            }
        }

        class asio_backend_win32;

        /// ASIO callbacks carry no user pointer, so the active instance must be reachable from a
        /// global. Claiming it with a compare-exchange means a second engine fails loudly with
        /// `device_busy` instead of silently stealing the first one's callbacks.
        std::atomic<asio_backend_win32 *> g_active_backend{nullptr};

        class asio_backend_win32 final : public backend
        {
        public:
            explicit asio_backend_win32(engine_config config)
                : config_(std::move(config)), dispatcher_(config_.callback, config_.user, stats_) {}

            ~asio_backend_win32() override { shutdown(); }

            std::string_view name() const noexcept override { return "ASIO"; }
            engine_backend kind() const noexcept override { return engine_backend::asio; }

            std::expected<std::vector<device_info>, audio_error> enumerate_devices() const override
            {
                std::vector<device_info> devices;

                try
                {
                    const auto drivers = asio::enumerate_installed_drivers();
                    devices.reserve(drivers.size());

                    for (const auto &driver : drivers)
                    {
                        device_info info;
                        info.backend = engine_backend::asio;
                        // The CLSID is stable across driver renames and unique per driver; the
                        // display name is neither.
                        info.id = guid_to_string(driver.clsid);
                        info.name = wide_to_utf8(driver.name);
                        info.is_default = devices.empty();
                        devices.push_back(std::move(info));
                    }
                }
                catch (...)
                {
                    return std::unexpected(audio_error::platform_error);
                }

                return devices;
            }

            std::expected<void, audio_error> initialize() override
            {
                shutdown();

                const HRESULT com_hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                if (FAILED(com_hr) && com_hr != RPC_E_CHANGED_MODE)
                    return std::unexpected(audio_error::platform_error);
                owns_com_ = SUCCEEDED(com_hr);

                // Only one ASIO stream can own the global callback slot.
                asio_backend_win32 *unclaimed = nullptr;
                if (!g_active_backend.compare_exchange_strong(unclaimed, this))
                    return std::unexpected(audio_error::device_busy);
                claimed_ = true;

                try
                {
                    if (const auto opened = open_driver(); !opened)
                        return std::unexpected(opened.error());

                    if (const auto configured = configure_stream(); !configured)
                        return std::unexpected(configured.error());
                }
                catch (...)
                {
                    return std::unexpected(audio_error::platform_error);
                }

                dispatcher_.reset_time();
                stats_.reset();
                initialized_ = true;
                return {};
            }

            std::expected<void, audio_error> start() override
            {
                if (!initialized_ || !driver_.has_instance())
                    return std::unexpected(audio_error::not_initialized);
                if (running_.load(std::memory_order_acquire))
                    return {};

                try
                {
                    // Fill both halves of the double buffer before the driver's clock starts.
                    render_block(0);
                    render_block(1);

                    if (driver_.get()->start() != 0)
                        return std::unexpected(audio_error::platform_error);
                }
                catch (...)
                {
                    return std::unexpected(audio_error::platform_error);
                }

                running_.store(true, std::memory_order_release);
                return {};
            }

            void stop() noexcept override
            {
                if (!running_.exchange(false, std::memory_order_acq_rel))
                    return;

                if (driver_.has_instance())
                {
                    try
                    {
                        (void)driver_.get()->stop();
                    }
                    catch (...)
                    {
                    }
                }
            }

            void shutdown() noexcept override
            {
                stop();

                // Stop receiving callbacks before the buffers they read are released.
                if (claimed_)
                {
                    auto *self = this;
                    (void)g_active_backend.compare_exchange_strong(self, nullptr);
                    claimed_ = false;
                }

                if (driver_.has_instance())
                {
                    try
                    {
                        (void)driver_.get()->dispose_buffers();
                    }
                    catch (...)
                    {
                    }
                }

                driver_.unload();

                buffer_infos_.clear();
                output_converters_.clear();
                input_converters_.clear();
                output_interleaved_.clear();
                input_interleaved_.clear();

                buffer_frames_ = 0;
                output_channels_ = 0;
                input_channels_ = 0;
                sample_rate_ = 0;
                output_latency_frames_ = 0;
                input_latency_frames_ = 0;
                device_id_.clear();
                device_name_.clear();
                initialized_ = false;

                if (owns_com_)
                {
                    CoUninitialize();
                    owns_com_ = false;
                }
            }

            bool is_running() const noexcept override
            {
                return running_.load(std::memory_order_acquire);
            }

            stream_info info() const override
            {
                stream_info out;
                out.backend = engine_backend::asio;
                out.direction = config_.direction;
                out.sample_rate = sample_rate_;
                out.output_channels = static_cast<uint32_t>(output_channels_);
                out.input_channels = static_cast<uint32_t>(input_channels_);
                out.buffer_frames = static_cast<uint32_t>(buffer_frames_);

                if (sample_rate_ != 0)
                {
                    const double rate = static_cast<double>(sample_rate_);
                    out.output_latency_seconds = static_cast<double>(output_latency_frames_) / rate;
                    out.input_latency_seconds = static_cast<double>(input_latency_frames_) / rate;
                }

                // ASIO always owns the device outright.
                out.exclusive = true;
                out.device_id = device_id_;
                out.device_name = device_name_;
                return out;
            }

            stream_stats stats() const noexcept override { return stats_.snapshot(); }
            void reset_stats() noexcept override { stats_.reset(); }

        private:
            std::expected<void, audio_error> open_driver()
            {
                std::optional<asio::installed_driver> selected;

                const auto drivers = asio::enumerate_installed_drivers();
                if (drivers.empty())
                    return std::unexpected(audio_error::no_device);

                if (!config_.preferred_device.empty())
                {
                    // Match the stable CLSID first, then fall back to the display name.
                    for (const auto &driver : drivers)
                    {
                        if (guid_to_string(driver.clsid) == config_.preferred_device)
                        {
                            selected = driver;
                            break;
                        }
                    }

                    if (!selected)
                    {
                        const auto wanted = utf8_to_wide(config_.preferred_device);
                        if (!wanted.empty())
                            selected = asio::find_installed_driver(wanted);
                    }

                    if (!selected)
                        return std::unexpected(audio_error::no_device);
                }
                else
                {
                    selected = drivers.front();
                }

                driver_.load_library(selected->dll_path);
                driver_.create_instance(selected->clsid);
                if (!driver_.has_instance())
                    return std::unexpected(audio_error::no_device);

                // ASIO wants a platform system handle; a desktop window works for most drivers.
                if (driver_.get()->init(GetDesktopWindow()) == 0)
                    return std::unexpected(audio_error::platform_error);

                device_id_ = guid_to_string(selected->clsid);
                device_name_ = wide_to_utf8(selected->name);
                return {};
            }

            std::expected<void, audio_error> configure_stream()
            {
                auto *driver = driver_.get();

                // Sample rate. `can_sample_rate` returns ASE_OK (0) when the rate is available.
                if (config_.sample_rate != 0 &&
                    driver->can_sample_rate(static_cast<asio::asio_sample_rate>(config_.sample_rate)) == 0)
                {
                    (void)driver->set_sample_rate(static_cast<asio::asio_sample_rate>(config_.sample_rate));
                }

                // Read back what the driver settled on rather than assuming the request stuck.
                asio::asio_sample_rate actual_rate = 0.0;
                if (driver->get_sample_rate(&actual_rate) != 0 || actual_rate <= 0.0)
                    return std::unexpected(audio_error::platform_error);

                sample_rate_ = static_cast<uint32_t>(actual_rate + 0.5);

                if (!config_.allow_format_fallback && sample_rate_ != config_.sample_rate)
                    return std::unexpected(audio_error::format_unsupported);

                int32_t available_inputs = 0;
                int32_t available_outputs = 0;
                if (driver->get_channels(&available_inputs, &available_outputs) != 0)
                    return std::unexpected(audio_error::platform_error);

                const bool wants_output = config_.direction == stream_direction::output ||
                                          config_.direction == stream_direction::duplex;
                const bool wants_input = config_.direction == stream_direction::input ||
                                         config_.direction == stream_direction::duplex;

                output_channels_ = wants_output
                                       ? std::min<int32_t>(static_cast<int32_t>(config_.output_channels), available_outputs)
                                       : 0;
                input_channels_ = wants_input
                                      ? std::min<int32_t>(static_cast<int32_t>(config_.input_channels), available_inputs)
                                      : 0;

                if (output_channels_ <= 0 && input_channels_ <= 0)
                    return std::unexpected(audio_error::no_device);

                if (!config_.allow_format_fallback &&
                    ((wants_output && output_channels_ != static_cast<int32_t>(config_.output_channels)) ||
                     (wants_input && input_channels_ != static_cast<int32_t>(config_.input_channels))))
                {
                    return std::unexpected(audio_error::format_unsupported);
                }

                // Buffer size, clamped to the driver's advertised range and granularity.
                int32_t minimum = 0;
                int32_t maximum = 0;
                int32_t preferred = 0;
                int32_t granularity = 0;
                if (driver->get_buffer_size(&minimum, &maximum, &preferred, &granularity) != 0)
                    return std::unexpected(audio_error::platform_error);

                int32_t requested = static_cast<int32_t>(config_.frames_per_buffer);
                if (requested <= 0)
                    requested = preferred;

                requested = std::clamp(requested, minimum, maximum);
                if (requested <= 0)
                    return std::unexpected(audio_error::platform_error);

                buffer_frames_ = requested;

                if (const auto prepared = create_buffers(); !prepared)
                    return std::unexpected(prepared.error());

                int32_t input_latency = 0;
                int32_t output_latency = 0;
                if (driver->get_latencies(&input_latency, &output_latency) == 0)
                {
                    input_latency_frames_ = input_latency;
                    output_latency_frames_ = output_latency;
                }

                return {};
            }

            std::expected<void, audio_error> create_buffers()
            {
                auto *driver = driver_.get();

                const size_t total = static_cast<size_t>(output_channels_) +
                                     static_cast<size_t>(input_channels_);

                buffer_infos_.assign(total, asio::asio_buffer_info{});

                // Outputs occupy [0, output_channels_), inputs follow.
                for (int32_t channel = 0; channel < output_channels_; ++channel)
                {
                    auto &info = buffer_infos_[static_cast<size_t>(channel)];
                    info.is_input = 0;
                    info.channel_num = channel;
                    info.buffers[0] = nullptr;
                    info.buffers[1] = nullptr;
                }

                for (int32_t channel = 0; channel < input_channels_; ++channel)
                {
                    auto &info = buffer_infos_[static_cast<size_t>(output_channels_ + channel)];
                    info.is_input = 1;
                    info.channel_num = channel;
                    info.buffers[0] = nullptr;
                    info.buffers[1] = nullptr;
                }

                // Resolve every channel's conversion before the stream can call back.
                output_converters_.assign(static_cast<size_t>(output_channels_), nullptr);
                for (int32_t channel = 0; channel < output_channels_; ++channel)
                {
                    asio::asio_channel_info channel_info{};
                    channel_info.channel = channel;
                    channel_info.is_input = 0;
                    if (driver->get_channel_info(&channel_info) != 0)
                        return std::unexpected(audio_error::platform_error);

                    const auto converter = pick_deinterleave(channel_info.sample_type);
                    if (!converter)
                        return std::unexpected(audio_error::format_unsupported);

                    output_converters_[static_cast<size_t>(channel)] = converter;
                }

                input_converters_.assign(static_cast<size_t>(input_channels_), nullptr);
                for (int32_t channel = 0; channel < input_channels_; ++channel)
                {
                    asio::asio_channel_info channel_info{};
                    channel_info.channel = channel;
                    channel_info.is_input = 1;
                    if (driver->get_channel_info(&channel_info) != 0)
                        return std::unexpected(audio_error::platform_error);

                    const auto converter = pick_interleave(channel_info.sample_type);
                    if (!converter)
                        return std::unexpected(audio_error::format_unsupported);

                    input_converters_[static_cast<size_t>(channel)] = converter;
                }

                output_interleaved_.assign(
                    static_cast<size_t>(buffer_frames_) * static_cast<size_t>(output_channels_), 0.0f);
                input_interleaved_.assign(
                    static_cast<size_t>(buffer_frames_) * static_cast<size_t>(input_channels_), 0.0f);

                callbacks_ = {};
                callbacks_.buffer_switch = &asio_backend_win32::on_buffer_switch;
                callbacks_.sample_rate_did_change = &asio_backend_win32::on_sample_rate_changed;
                callbacks_.asio_message = &asio_backend_win32::on_message;
                callbacks_.buffer_switch_time_info = &asio_backend_win32::on_buffer_switch_time_info;

                if (driver->create_buffers(
                        buffer_infos_.data(),
                        static_cast<int32_t>(total),
                        buffer_frames_,
                        &callbacks_) != 0)
                {
                    return std::unexpected(audio_error::platform_error);
                }

                return {};
            }

            /// Real-time path. Converts input, runs the callback, converts output.
            void render_block(int32_t half) noexcept
            {
                if (buffer_frames_ <= 0 || half < 0 || half > 1)
                    return;

                if (input_channels_ > 0)
                {
                    for (int32_t channel = 0; channel < input_channels_; ++channel)
                    {
                        const auto &info = buffer_infos_[static_cast<size_t>(output_channels_ + channel)];
                        const auto *source = static_cast<const uint8_t *>(info.buffers[half]);
                        if (!source)
                            continue;

                        input_converters_[static_cast<size_t>(channel)](
                            input_interleaved_.data() + channel,
                            source,
                            buffer_frames_,
                            input_channels_);
                    }
                }

                dispatcher_.dispatch(
                    output_channels_ > 0 ? output_interleaved_.data() : nullptr,
                    input_channels_ > 0 ? input_interleaved_.data() : nullptr,
                    static_cast<uint32_t>(buffer_frames_),
                    static_cast<uint32_t>(output_channels_),
                    static_cast<uint32_t>(input_channels_),
                    sample_rate_);

                for (int32_t channel = 0; channel < output_channels_; ++channel)
                {
                    const auto &info = buffer_infos_[static_cast<size_t>(channel)];
                    auto *destination = static_cast<uint8_t *>(info.buffers[half]);
                    if (!destination)
                        continue;

                    output_converters_[static_cast<size_t>(channel)](
                        destination,
                        output_interleaved_.data() + channel,
                        buffer_frames_,
                        output_channels_);
                }
            }

            static void on_buffer_switch(int32_t half, int32_t direct_process) noexcept
            {
                (void)direct_process;

                auto *self = g_active_backend.load(std::memory_order_acquire);
                if (!self)
                    return;

                self->render_block(half);

                if (self->driver_.has_instance())
                    (void)self->driver_.get()->output_ready();
            }

            static void on_sample_rate_changed(asio::asio_sample_rate sample_rate) noexcept
            {
                auto *self = g_active_backend.load(std::memory_order_acquire);
                if (!self || sample_rate <= 0.0)
                    return;

                self->sample_rate_ = static_cast<uint32_t>(sample_rate + 0.5);
                self->stats_.add_device_change();
            }

            static int32_t on_message(int32_t selector, int32_t value, void *message, double *opt) noexcept
            {
                (void)value;
                (void)message;
                (void)opt;

                // kAsioSelectorSupported / kAsioEngineVersion from the ASIO SDK.
                constexpr int32_t selector_supported = 1;
                constexpr int32_t engine_version = 2;
                constexpr int32_t reset_request = 3;

                switch (selector)
                {
                case selector_supported:
                    return 1;
                case engine_version:
                    return 2;
                case reset_request:
                    if (auto *self = g_active_backend.load(std::memory_order_acquire))
                        self->stats_.add_device_change();
                    return 1;
                default:
                    return 0;
                }
            }

            static asio::asio_time *on_buffer_switch_time_info(
                asio::asio_time *params, int32_t half, int32_t direct_process) noexcept
            {
                on_buffer_switch(half, direct_process);
                return params;
            }

            engine_config config_{};
            stats_block stats_;
            render_dispatcher dispatcher_;

            asio::driver driver_;
            asio::asio_callbacks callbacks_{};

            bool owns_com_ = false;
            bool initialized_ = false;
            bool claimed_ = false;
            std::atomic<bool> running_{false};

            int32_t buffer_frames_ = 0;
            int32_t output_channels_ = 0;
            int32_t input_channels_ = 0;
            uint32_t sample_rate_ = 0;
            int32_t output_latency_frames_ = 0;
            int32_t input_latency_frames_ = 0;

            std::vector<asio::asio_buffer_info> buffer_infos_;
            std::vector<deinterleave_fn> output_converters_;
            std::vector<interleave_fn> input_converters_;
            std::vector<float> output_interleaved_;
            std::vector<float> input_interleaved_;

            std::string device_id_;
            std::string device_name_;
        };

    } // namespace

    std::unique_ptr<backend> create_asio_backend_win32(const engine_config &config) noexcept
    {
        return std::make_unique<asio_backend_win32>(config);
    }

} // namespace catalyst::audio::detail

#endif // _WIN32
