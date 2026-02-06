#include "wasapi_backend_win32.hpp"

#if !defined(_WIN32)

namespace catalyst::audio::detail
{
    std::unique_ptr<backend> create_wasapi_backend_win32(const engine_config &) noexcept
    {
        return nullptr;
    }
}

#else

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif

#include <windows.h>

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include <ks.h>
#include <ksmedia.h>

#include <atomic>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace catalyst::audio::detail
{

    namespace
    {
        using Microsoft::WRL::ComPtr;

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

        DWORD channel_mask_for(uint32_t channels)
        {
            switch (channels)
            {
            case 1:
                return SPEAKER_FRONT_CENTER;
            case 2:
                return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
            default:
                return 0;
            }
        }

        bool is_float32_format(const WAVEFORMATEX *fmt) noexcept
        {
            if (!fmt)
                return false;

            if (fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
            {
                return fmt->wBitsPerSample == 32;
            }

            if (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE && fmt->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))
            {
                const auto *ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(fmt);
                return ext->Format.wBitsPerSample == 32 && ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
            }

            return false;
        }

        std::unique_ptr<WAVEFORMATEX, void (*)(WAVEFORMATEX *)> make_desired_format(const engine_config &cfg)
        {
            auto *ext = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(CoTaskMemAlloc(sizeof(WAVEFORMATEXTENSIBLE)));
            if (!ext)
                return {nullptr, +[](WAVEFORMATEX *) {}};

            std::memset(ext, 0, sizeof(WAVEFORMATEXTENSIBLE));

            ext->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
            ext->Format.nChannels = static_cast<WORD>(cfg.channels);
            ext->Format.nSamplesPerSec = cfg.sample_rate;
            ext->Format.wBitsPerSample = 32;
            ext->Format.nBlockAlign = static_cast<WORD>(cfg.channels * sizeof(float));
            ext->Format.nAvgBytesPerSec = ext->Format.nSamplesPerSec * ext->Format.nBlockAlign;
            ext->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

            ext->Samples.wValidBitsPerSample = 32;
            ext->dwChannelMask = channel_mask_for(cfg.channels);
            ext->SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

            return {reinterpret_cast<WAVEFORMATEX *>(ext), +[](WAVEFORMATEX *p) {
                        if (p)
                            CoTaskMemFree(p);
                    }};
        }

        REFERENCE_TIME hns_from_frames(uint32_t frames, uint32_t sample_rate)
        {
            if (sample_rate == 0)
                sample_rate = 48000;
            if (frames == 0)
                frames = 512;

            // 10,000,000 x 100ns per second
            const double seconds = static_cast<double>(frames) / static_cast<double>(sample_rate);
            const auto hns = static_cast<REFERENCE_TIME>(seconds * 10000000.0);
            return (hns > 0) ? hns : 1;
        }

        void render_silence(float *output_interleaved, uint32_t frames, uint32_t channels) noexcept
        {
            const uint32_t count = frames * channels;
            for (uint32_t i = 0; i < count; ++i)
                output_interleaved[i] = 0.0f;
        }

    } // namespace

    class wasapi_backend_win32 final : public backend
    {
    public:
        explicit wasapi_backend_win32(engine_config config) : config_(config) {}
        ~wasapi_backend_win32() override { shutdown(); }

        std::string_view name() const noexcept override { return "WASAPI Backend Win32"; }

        std::vector<std::string_view> enumerate_devices() const override
        {
            if (!devices_cached_)
            {
                devices_cached_ = true;
                device_names_utf8_.clear();

                const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                const bool did_init = SUCCEEDED(hr);

                try
                {
                    ComPtr<IMMDeviceEnumerator> enumerator;
                    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator))))
                        throw 1;

                    ComPtr<IMMDeviceCollection> collection;
                    if (FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection)))
                        throw 1;

                    UINT count = 0;
                    if (FAILED(collection->GetCount(&count)))
                        throw 1;

                    device_names_utf8_.reserve(count);
                    for (UINT i = 0; i < count; ++i)
                    {
                        ComPtr<IMMDevice> device;
                        if (FAILED(collection->Item(i, &device)) || !device)
                            continue;

                        ComPtr<IPropertyStore> props;
                        if (FAILED(device->OpenPropertyStore(STGM_READ, &props)) || !props)
                            continue;

                        PROPVARIANT pv;
                        PropVariantInit(&pv);
                        if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &pv)) && pv.vt == VT_LPWSTR && pv.pwszVal)
                        {
                            device_names_utf8_.push_back(wide_to_utf8(pv.pwszVal));
                        }
                        PropVariantClear(&pv);
                    }
                }
                catch (...)
                {
                    device_names_utf8_.clear();
                }

                if (did_init)
                    CoUninitialize();
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

            ComPtr<IMMDeviceEnumerator> enumerator;
            if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator))))
                return false;

            if (!config_.preferred_device.empty())
            {
                // Select by friendly name.
                ComPtr<IMMDeviceCollection> collection;
                if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection)) && collection)
                {
                    UINT count = 0;
                    if (SUCCEEDED(collection->GetCount(&count)))
                    {
                        for (UINT i = 0; i < count; ++i)
                        {
                            ComPtr<IMMDevice> candidate;
                            if (FAILED(collection->Item(i, &candidate)) || !candidate)
                                continue;

                            ComPtr<IPropertyStore> props;
                            if (FAILED(candidate->OpenPropertyStore(STGM_READ, &props)) || !props)
                                continue;

                            PROPVARIANT pv;
                            PropVariantInit(&pv);
                            const bool ok = SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &pv)) && pv.vt == VT_LPWSTR && pv.pwszVal;
                            if (ok)
                            {
                                const auto name = wide_to_utf8(pv.pwszVal);
                                if (name == config_.preferred_device)
                                {
                                    device_ = candidate;
                                    PropVariantClear(&pv);
                                    break;
                                }
                            }
                            PropVariantClear(&pv);
                        }
                    }
                }
            }

            if (!device_)
            {
                if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device_)) || !device_)
                    return false;
            }

            if (FAILED(device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(audio_client_.GetAddressOf()))) || !audio_client_)
                return false;

            WAVEFORMATEX *mix = nullptr;
            if (FAILED(audio_client_->GetMixFormat(&mix)) || !mix)
                return false;
            mix_format_.reset(mix);

            // Prefer requested float32 format if supported; otherwise fall back to mix format.
            WAVEFORMATEX *closest = nullptr;
            auto desired = make_desired_format(config_);
            if (desired.get())
            {
                const HRESULT is_supported = audio_client_->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, desired.get(), &closest);
                if (is_supported == S_OK)
                {
                    active_format_.reset(desired.release());
                }
                else if (is_supported == S_FALSE && closest)
                {
                    // Use closest only if it's float32.
                    if (is_float32_format(closest))
                        active_format_.reset(closest);
                    else
                        CoTaskMemFree(closest);
                }
            }

            if (!active_format_)
            {
                // Fall back to mix format if it's float32.
                if (!is_float32_format(mix_format_.get()))
                    return false;

                // Make a copy we own in active_format_.
                const size_t bytes = sizeof(WAVEFORMATEX) + mix_format_->cbSize;
                auto *copy = reinterpret_cast<WAVEFORMATEX *>(CoTaskMemAlloc(bytes));
                if (!copy)
                    return false;
                std::memcpy(copy, mix_format_.get(), bytes);
                active_format_.reset(copy);
            }

            const REFERENCE_TIME buffer_hns = hns_from_frames(config_.frames_per_buffer, active_format_->nSamplesPerSec);

            DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

            if (FAILED(audio_client_->Initialize(
                    AUDCLNT_SHAREMODE_SHARED,
                    flags,
                    buffer_hns,
                    0,
                    active_format_.get(),
                    nullptr)))
            {
                return false;
            }

            if (FAILED(audio_client_->GetBufferSize(&buffer_frames_)))
                return false;

            event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (!event_)
                return false;

            if (FAILED(audio_client_->SetEventHandle(event_)))
                return false;

            if (FAILED(audio_client_->GetService(IID_PPV_ARGS(&render_client_))) || !render_client_)
                return false;

            initialized_ = true;
            return true;
        }

        bool start() override
        {
            if (!initialized_ || running_)
                return running_;

            stopping_.store(false);

            // Prime the buffer before starting.
            if (!render_available_frames())
                return false;

            if (FAILED(audio_client_->Start()))
                return false;

            running_ = true;
            thread_ = std::thread([this] {
                const HRESULT thr_hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                const bool thr_com_init = SUCCEEDED(thr_hr);

                while (!stopping_.load())
                {
                    const DWORD rc = WaitForSingleObject(event_, 2000);
                    if (rc == WAIT_OBJECT_0)
                    {
                        (void)render_available_frames();
                    }
                }

                if (thr_com_init)
                    CoUninitialize();
            });

            return true;
        }

        void stop() noexcept override
        {
            if (!running_)
                return;

            stopping_.store(true);
            if (event_)
                SetEvent(event_);

            if (thread_.joinable())
                thread_.join();

            if (audio_client_)
                (void)audio_client_->Stop();

            running_ = false;
        }

        bool is_running() const noexcept override
        {
            return running_;
        }

        void shutdown() noexcept override
        {
            stop();

            render_client_.Reset();
            audio_client_.Reset();
            device_.Reset();

            active_format_.reset();
            mix_format_.reset();

            if (event_)
            {
                CloseHandle(event_);
                event_ = nullptr;
            }

            if (did_com_init_)
            {
                CoUninitialize();
                did_com_init_ = false;
            }

            initialized_ = false;
        }

    private:
        bool render_available_frames() noexcept
        {
            if (!audio_client_ || !render_client_ || !active_format_)
                return false;

            UINT32 padding = 0;
            if (FAILED(audio_client_->GetCurrentPadding(&padding)))
                return false;

            if (padding >= buffer_frames_)
                return true;

            const UINT32 frames_available = buffer_frames_ - padding;
            if (frames_available == 0)
                return true;

            BYTE *data = nullptr;
            if (FAILED(render_client_->GetBuffer(frames_available, &data)) || !data)
                return false;

            const uint32_t channels = static_cast<uint32_t>(active_format_->nChannels);
            float *out = reinterpret_cast<float *>(data);

            if (config_.callback)
                config_.callback(config_.user, out, frames_available, channels);
            else
                render_silence(out, frames_available, channels);

            (void)render_client_->ReleaseBuffer(frames_available, 0);
            return true;
        }

        engine_config config_{};

        bool did_com_init_ = false;
        bool initialized_ = false;
        bool running_ = false;
        std::atomic<bool> stopping_{false};

        ComPtr<IMMDevice> device_;
        ComPtr<IAudioClient> audio_client_;
        ComPtr<IAudioRenderClient> render_client_;

        struct co_taskmem_free
        {
            void operator()(WAVEFORMATEX *p) const noexcept
            {
                if (p)
                    CoTaskMemFree(p);
            }
        };

        std::unique_ptr<WAVEFORMATEX, co_taskmem_free> mix_format_{nullptr};
        std::unique_ptr<WAVEFORMATEX, co_taskmem_free> active_format_{nullptr};

        UINT32 buffer_frames_ = 0;
        HANDLE event_ = nullptr;
        std::thread thread_;

        mutable bool devices_cached_ = false;
        mutable std::vector<std::string> device_names_utf8_;
    };

    std::unique_ptr<backend> create_wasapi_backend_win32(const engine_config &config) noexcept
    {
        return std::make_unique<wasapi_backend_win32>(config);
    }

} // namespace catalyst::audio::detail

#endif
