#include "../detail_backend.hpp"

#include "asio_loader.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

#include <objbase.h>

namespace catalyst::audio::detail
{

    class asio_backend_win32;

    namespace
    {
        std::wstring utf8_to_wide(std::string_view s)
        {
            if (s.empty())
                return {};

            const int needed = MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                s.data(),
                static_cast<int>(s.size()),
                nullptr,
                0);

            if (needed <= 0)
                return {};

            std::wstring out;
            out.resize(static_cast<size_t>(needed));

            const int written = MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                s.data(),
                static_cast<int>(s.size()),
                out.data(),
                needed);

            if (written <= 0)
                return {};

            return out;
        }

        std::string wide_to_utf8(const std::wstring &w)
        {
            if (w.empty())
                return {};

            const int needed = WideCharToMultiByte(
                CP_UTF8,
                0,
                w.c_str(),
                static_cast<int>(w.size()),
                nullptr,
                0,
                nullptr,
                nullptr);

            if (needed <= 0)
                return {};

            std::string out;
            out.resize(static_cast<size_t>(needed));

            const int written = WideCharToMultiByte(
                CP_UTF8,
                0,
                w.c_str(),
                static_cast<int>(w.size()),
                out.data(),
                needed,
                nullptr,
                nullptr);

            if (written <= 0)
                return {};

            return out;
        }

        float clamp01(float x) noexcept
        {
            if (x < -1.0f)
                return -1.0f;
            if (x > 1.0f)
                return 1.0f;
            return x;
        }

        int16_t float_to_i16(float x) noexcept
        {
            const float c = clamp01(x);
            const float scaled = c * 32767.0f;
            return static_cast<int16_t>(std::lrintf(scaled));
        }

        int32_t float_to_i32(float x) noexcept
        {
            const float c = clamp01(x);
            const double scaled = static_cast<double>(c) * 2147483647.0;
            return static_cast<int32_t>(std::llround(scaled));
        }

        int32_t float_to_i24(float x) noexcept
        {
            const float c = clamp01(x);
            const double scaled = static_cast<double>(c) * 8388607.0;
            return static_cast<int32_t>(std::llround(scaled));
        }

        void write_i24_lsb(uint8_t *dst, int32_t v) noexcept
        {
            // v is expected to fit in signed 24-bit.
            dst[0] = static_cast<uint8_t>(v & 0xFF);
            dst[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
            dst[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
        }

        void byteswap16(uint8_t *p) noexcept
        {
            std::swap(p[0], p[1]);
        }

        void byteswap32(uint8_t *p) noexcept
        {
            std::swap(p[0], p[3]);
            std::swap(p[1], p[2]);
        }

        void byteswap64(uint8_t *p) noexcept
        {
            std::swap(p[0], p[7]);
            std::swap(p[1], p[6]);
            std::swap(p[2], p[5]);
            std::swap(p[3], p[4]);
        }

        std::atomic<asio_backend_win32 *> g_active_asio_backend{nullptr};
    }

    class asio_backend_win32 final : public backend
    {
    private:
        engine_config config_{};

        asio::driver asio_driver_;
        bool did_com_init_ = false;
        std::atomic<bool> running_{false};

        int32_t buffer_size_frames_ = 0;
        int32_t output_channels_ = 0;
        std::vector<asio::asio_buffer_info> buffer_infos_;
        std::vector<asio::asio_channel_info> channel_infos_;
        std::vector<float> interleaved_;

        asio::asio_callbacks callbacks_{};

        mutable bool devices_cached_ = false;
        mutable std::vector<std::string> device_names_utf8_;

    public:
        explicit asio_backend_win32(engine_config config) : config_(std::move(config)) {}
        ~asio_backend_win32() override { shutdown(); }

        std::string_view name() const noexcept override
        {
            return "ASIO Backend Win32";
        }

        std::vector<std::string_view> enumerate_devices() const override
        {
            if (!devices_cached_)
            {
                devices_cached_ = true;
                device_names_utf8_.clear();

                try
                {
                    auto drivers = asio::enumerate_installed_drivers();
                    device_names_utf8_.reserve(drivers.size());
                    for (const auto &driver : drivers)
                        device_names_utf8_.push_back(wide_to_utf8(driver.name));
                }
                catch (...)
                {
                    device_names_utf8_.clear();
                }
            }

            std::vector<std::string_view> out;
            out.reserve(device_names_utf8_.size());
            for (const auto &s : device_names_utf8_)
                out.emplace_back(s);
            return out;
        }

        bool initialize() override
        {
            shutdown();

            const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
                return false;
            did_com_init_ = SUCCEEDED(hr);

            try
            {
                std::optional<asio::installed_driver> driver_info;
                if (!config_.preferred_device.empty())
                {
                    const auto preferredW = utf8_to_wide(config_.preferred_device);
                    if (!preferredW.empty())
                        driver_info = asio::find_installed_driver(preferredW);
                }

                if (!driver_info)
                {
                    auto drivers = asio::enumerate_installed_drivers();
                    if (drivers.empty())
                        return false;
                    driver_info = drivers.front();
                }

                asio_driver_.load_library(driver_info->dll_path);
                asio_driver_.create_instance(driver_info->clsid);
                if (!asio_driver_.has_instance())
                    return false;

                // ASIO expects a platform-specific system handle. A desktop window handle works for most drivers.
                const auto ok = (asio_driver_.get()->init(GetDesktopWindow()) != 0);
                if (!ok)
                    return false;

                // Sample rate
                if (config_.sample_rate != 0)
                {
                    if (asio_driver_.get()->can_sample_rate(static_cast<asio::asio_sample_rate>(config_.sample_rate)) == 0)
                    {
                        (void)asio_driver_.get()->set_sample_rate(static_cast<asio::asio_sample_rate>(config_.sample_rate));
                    }
                }

                int32_t num_in = 0;
                int32_t num_out = 0;
                if (asio_driver_.get()->get_channels(&num_in, &num_out) != 0)
                    return false;
                if (num_out <= 0)
                    return false;

                output_channels_ = std::min<int32_t>(static_cast<int32_t>(config_.channels ? config_.channels : 2), num_out);
                if (output_channels_ <= 0)
                    output_channels_ = std::min<int32_t>(2, num_out);

                int32_t min_size = 0, max_size = 0, preferred_size = 0, granularity = 0;
                if (asio_driver_.get()->get_buffer_size(&min_size, &max_size, &preferred_size, &granularity) != 0)
                    return false;

                int32_t desired = static_cast<int32_t>(config_.frames_per_buffer ? config_.frames_per_buffer : 0);
                if (desired <= 0)
                    desired = preferred_size;
                desired = std::max(desired, min_size);
                desired = std::min(desired, max_size);
                if (desired <= 0)
                    return false;

                buffer_size_frames_ = desired;

                buffer_infos_.clear();
                buffer_infos_.resize(static_cast<size_t>(output_channels_));
                for (int32_t c = 0; c < output_channels_; ++c)
                {
                    auto &bi = buffer_infos_[static_cast<size_t>(c)];
                    bi.is_input = 0;
                    bi.channel_num = c;
                    bi.buffers[0] = nullptr;
                    bi.buffers[1] = nullptr;
                }

                channel_infos_.clear();
                channel_infos_.resize(static_cast<size_t>(output_channels_));
                for (int32_t c = 0; c < output_channels_; ++c)
                {
                    asio::asio_channel_info info{};
                    info.channel = c;
                    info.is_input = 0;
                    if (asio_driver_.get()->get_channel_info(&info) != 0)
                        return false;
                    channel_infos_[static_cast<size_t>(c)] = info;

                    const auto t = info.sample_type;
                    const bool supported =
                        (t == asio::asio_sample_type::float32_lsb) ||
                        (t == asio::asio_sample_type::float64_lsb) ||
                        (t == asio::asio_sample_type::int16_lsb) ||
                        (t == asio::asio_sample_type::int24_lsb) ||
                        (t == asio::asio_sample_type::int32_lsb) ||
                        (t == asio::asio_sample_type::float32_msb) ||
                        (t == asio::asio_sample_type::float64_msb) ||
                        (t == asio::asio_sample_type::int16_msb) ||
                        (t == asio::asio_sample_type::int24_msb) ||
                        (t == asio::asio_sample_type::int32_msb);
                    if (!supported)
                        return false;
                }

                interleaved_.assign(static_cast<size_t>(buffer_size_frames_) * static_cast<size_t>(output_channels_), 0.0f);

                callbacks_ = {};
                callbacks_.buffer_switch = &asio_backend_win32::buffer_switch;
                callbacks_.sample_rate_did_change = &asio_backend_win32::sample_rate_did_change;
                callbacks_.asio_message = &asio_backend_win32::asio_message;
                callbacks_.buffer_switch_time_info = &asio_backend_win32::buffer_switch_time_info;

                if (asio_driver_.get()->create_buffers(
                        buffer_infos_.data(),
                        output_channels_,
                        buffer_size_frames_,
                        &callbacks_) != 0)
                {
                    return false;
                }

                g_active_asio_backend.store(this);
                return true;
            }
            catch (...)
            {
                shutdown();
                return false;
            }
        }

        bool start() override
        {
            if (running_.load())
                return true;
            if (!asio_driver_.has_instance())
                return false;

            try
            {
                // Prime both buffers.
                on_buffer_switch(0);
                on_buffer_switch(1);

                const auto rc = asio_driver_.get()->start();
                if (rc != 0)
                    return false;

                running_.store(true);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        void stop() noexcept override
        {
            if (!running_.load())
                return;

            if (asio_driver_.has_instance())
            {
                try
                {
                    (void)asio_driver_.get()->stop();
                }
                catch (...)
                {
                }
            }

            running_.store(false);
        }

        bool is_running() const noexcept override
        {
            return running_.load();
        }

        void shutdown() noexcept override
        {
            stop();

            auto *expected = this;
            (void)g_active_asio_backend.compare_exchange_strong(expected, nullptr);

            if (asio_driver_.has_instance())
            {
                try
                {
                    (void)asio_driver_.get()->dispose_buffers();
                }
                catch (...)
                {
                }
            }

            asio_driver_.unload();

            buffer_infos_.clear();
            channel_infos_.clear();
            interleaved_.clear();
            buffer_size_frames_ = 0;
            output_channels_ = 0;

            if (did_com_init_)
            {
                CoUninitialize();
                did_com_init_ = false;
            }
        }

    private:
        void on_buffer_switch(int32_t double_buffer_index) noexcept
        {
            if (buffer_size_frames_ <= 0 || output_channels_ <= 0)
                return;

            try
            {
                if (config_.callback)
                {
                    config_.callback(
                        config_.user,
                        interleaved_.data(),
                        static_cast<uint32_t>(buffer_size_frames_),
                        static_cast<uint32_t>(output_channels_));
                }
                else
                {
                    std::fill(interleaved_.begin(), interleaved_.end(), 0.0f);
                }
            }
            catch (...)
            {
                std::fill(interleaved_.begin(), interleaved_.end(), 0.0f);
            }

            // De-interleave into ASIO channel buffers.
            for (int32_t ch = 0; ch < output_channels_; ++ch)
            {
                auto &bi = buffer_infos_[static_cast<size_t>(ch)];
                auto *dst = static_cast<uint8_t *>(bi.buffers[double_buffer_index]);
                if (!dst)
                    continue;

                const auto type = channel_infos_[static_cast<size_t>(ch)].sample_type;

                if (type == asio::asio_sample_type::float32_lsb)
                {
                    auto *out = reinterpret_cast<float *>(dst);
                    for (int32_t f = 0; f < buffer_size_frames_; ++f)
                        out[f] = interleaved_[static_cast<size_t>(f) * static_cast<size_t>(output_channels_) + static_cast<size_t>(ch)];
                    continue;
                }

                if (type == asio::asio_sample_type::float64_lsb)
                {
                    auto *out = reinterpret_cast<double *>(dst);
                    for (int32_t f = 0; f < buffer_size_frames_; ++f)
                        out[f] = static_cast<double>(interleaved_[static_cast<size_t>(f) * static_cast<size_t>(output_channels_) + static_cast<size_t>(ch)]);
                    continue;
                }

                if (type == asio::asio_sample_type::int16_lsb)
                {
                    auto *out = reinterpret_cast<int16_t *>(dst);
                    for (int32_t f = 0; f < buffer_size_frames_; ++f)
                        out[f] = float_to_i16(interleaved_[static_cast<size_t>(f) * static_cast<size_t>(output_channels_) + static_cast<size_t>(ch)]);
                    continue;
                }

                if (type == asio::asio_sample_type::int32_lsb)
                {
                    auto *out = reinterpret_cast<int32_t *>(dst);
                    for (int32_t f = 0; f < buffer_size_frames_; ++f)
                        out[f] = float_to_i32(interleaved_[static_cast<size_t>(f) * static_cast<size_t>(output_channels_) + static_cast<size_t>(ch)]);
                    continue;
                }

                if (type == asio::asio_sample_type::int24_lsb)
                {
                    for (int32_t f = 0; f < buffer_size_frames_; ++f)
                    {
                        const auto v = float_to_i24(interleaved_[static_cast<size_t>(f) * static_cast<size_t>(output_channels_) + static_cast<size_t>(ch)]);
                        write_i24_lsb(dst + static_cast<size_t>(f) * 3, v);
                    }
                    continue;
                }

                // MSB formats: write LSB then byteswap.
                if (type == asio::asio_sample_type::float32_msb)
                {
                    auto *out = reinterpret_cast<float *>(dst);
                    for (int32_t f = 0; f < buffer_size_frames_; ++f)
                        out[f] = interleaved_[static_cast<size_t>(f) * static_cast<size_t>(output_channels_) + static_cast<size_t>(ch)];
                    for (int32_t f = 0; f < buffer_size_frames_; ++f)
                        byteswap32(reinterpret_cast<uint8_t *>(&out[f]));
                    continue;
                }

                if (type == asio::asio_sample_type::float64_msb)
                {
                    auto *out = reinterpret_cast<double *>(dst);
                    for (int32_t f = 0; f < buffer_size_frames_; ++f)
                        out[f] = static_cast<double>(interleaved_[static_cast<size_t>(f) * static_cast<size_t>(output_channels_) + static_cast<size_t>(ch)]);
                    for (int32_t f = 0; f < buffer_size_frames_; ++f)
                        byteswap64(reinterpret_cast<uint8_t *>(&out[f]));
                    continue;
                }

                if (type == asio::asio_sample_type::int16_msb)
                {
                    auto *out = reinterpret_cast<int16_t *>(dst);
                    for (int32_t f = 0; f < buffer_size_frames_; ++f)
                        out[f] = float_to_i16(interleaved_[static_cast<size_t>(f) * static_cast<size_t>(output_channels_) + static_cast<size_t>(ch)]);
                    for (int32_t f = 0; f < buffer_size_frames_; ++f)
                        byteswap16(reinterpret_cast<uint8_t *>(&out[f]));
                    continue;
                }

                if (type == asio::asio_sample_type::int32_msb)
                {
                    auto *out = reinterpret_cast<int32_t *>(dst);
                    for (int32_t f = 0; f < buffer_size_frames_; ++f)
                        out[f] = float_to_i32(interleaved_[static_cast<size_t>(f) * static_cast<size_t>(output_channels_) + static_cast<size_t>(ch)]);
                    for (int32_t f = 0; f < buffer_size_frames_; ++f)
                        byteswap32(reinterpret_cast<uint8_t *>(&out[f]));
                    continue;
                }

                if (type == asio::asio_sample_type::int24_msb)
                {
                    // Write LSB then reverse 3 bytes.
                    for (int32_t f = 0; f < buffer_size_frames_; ++f)
                    {
                        const auto v = float_to_i24(interleaved_[static_cast<size_t>(f) * static_cast<size_t>(output_channels_) + static_cast<size_t>(ch)]);
                        auto *p = dst + static_cast<size_t>(f) * 3;
                        write_i24_lsb(p, v);
                        std::swap(p[0], p[2]);
                    }
                    continue;
                }
            }

            try
            {
                (void)asio_driver_.get()->output_ready();
            }
            catch (...)
            {
            }
        }

        static void buffer_switch(int32_t double_buffer_index, int32_t direct_process)
        {
            (void)direct_process;
            if (auto *self = g_active_asio_backend.load())
                self->on_buffer_switch(double_buffer_index);
        }

        static void sample_rate_did_change(asio::asio_sample_rate sample_rate)
        {
            (void)sample_rate;
        }

        static int32_t asio_message(int32_t selector, int32_t value, void *message, double *opt)
        {
            (void)selector;
            (void)value;
            (void)message;
            (void)opt;
            return 0;
        }

        static asio::asio_time *buffer_switch_time_info(asio::asio_time *params, int32_t double_buffer_index, int32_t direct_process)
        {
            buffer_switch(double_buffer_index, direct_process);
            return params;
        }
    };

    std::unique_ptr<backend> create_asio_backend_win32(const engine_config &config) noexcept
    {
        return std::make_unique<asio_backend_win32>(config);
    }

} // namespace catalyst::audio::detail