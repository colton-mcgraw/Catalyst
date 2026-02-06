#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)

#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif

#  include <Windows.h>
#  include <winreg.h>

#else

// Stubs for non-Windows tooling / analysis builds.
#  ifndef WINAPI
#    define WINAPI
#  endif
#  ifndef __stdcall
#    define __stdcall
#  endif

using BOOL = int;
using HMODULE = void*;

struct GUID
{
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
};

#endif

namespace catalyst::audio::asio
{

    // -----------------------------------------------------------------------------
    // Minimal ASIO 2.x ABI (SDK-free)
    // Notes:
    // - Real ASIO drivers typically do NOT export `ASIOInit`/`ASIOStart` etc.
    //   Those functions are SDK convenience wrappers that forward to the driver's
    //   `IASIO` interface.
    // - To avoid COM runtime / CoCreateInstance, we load the driver's DLL and call
    //   `DllGetClassObject` directly.
    // -----------------------------------------------------------------------------

    using asio_bool = int32_t;
    using asio_error = int32_t;
    using asio_sample_rate = double;

    // Keep these as plain ints for ABI compatibility. (Exact enum values are not
    // required by the loader; they only matter when interpreting driver data.)
    enum class asio_sample_type : int32_t
    {
        unknown = -1,

        // Matches Steinberg ASIOSampleType values.
        int16_msb = 0,
        int24_msb = 1,
        int32_msb = 2,
        float32_msb = 3,
        float64_msb = 4,

        int32_msb16 = 8,
        int32_msb18 = 9,
        int32_msb20 = 10,
        int32_msb24 = 11,

        int16_lsb = 16,
        int24_lsb = 17,
        int32_lsb = 18,
        float32_lsb = 19,
        float64_lsb = 20,

        int32_lsb16 = 24,
        int32_lsb18 = 25,
        int32_lsb20 = 26,
        int32_lsb24 = 27,
    };

    struct asio_driver_info
    {
        int32_t asio_version;
        int32_t driver_version;
        char name[32];
        char error_message[124];
        void *sys_ref;
    };

    struct asio_channel_info
    {
        int32_t channel;
        asio_bool is_input;
        asio_bool is_active;
        int32_t channel_group;
        asio_sample_type sample_type;
        char name[32];
    };

    struct asio_time;

    struct asio_callbacks
    {
        void (*buffer_switch)(int32_t double_buffer_index, int32_t direct_process);
        void (*sample_rate_did_change)(asio_sample_rate sample_rate);
        int32_t (*asio_message)(int32_t selector, int32_t value, void *message, double *opt);
        asio_time *(*buffer_switch_time_info)(
            asio_time *params,
            int32_t double_buffer_index,
            int32_t direct_process);
    };

    struct asio_buffer_info
    {
        asio_bool is_input;
        int32_t channel_num;
        void *buffers[2];
    };

    // Minimal COM ABI (no COM runtime required)
    using hresult = long;
    using ulong = unsigned long;

    struct iunknown
    {
        virtual hresult __stdcall QueryInterface(const GUID &riid, void **ppv_object) = 0;
        virtual ulong __stdcall AddRef() = 0;
        virtual ulong __stdcall Release() = 0;
    };

    struct iclass_factory : iunknown
    {
        virtual hresult __stdcall CreateInstance(iunknown *outer, const GUID &riid, void **ppv_object) = 0;
        virtual hresult __stdcall LockServer(BOOL lock) = 0;
    };

    // IASIO interface (ASIO 2.x)
    // vtable order must match Steinberg's definition.
    struct iasio : iunknown
    {
        virtual asio_bool __stdcall init(void *sys_handle) = 0;
        virtual void __stdcall get_driver_name(char *name) = 0;
        virtual int32_t __stdcall get_driver_version() = 0;
        virtual void __stdcall get_error_message(char *string) = 0;

        virtual asio_error __stdcall start() = 0;
        virtual asio_error __stdcall stop() = 0;
        virtual asio_error __stdcall get_channels(int32_t *num_input_channels, int32_t *num_output_channels) = 0;
        virtual asio_error __stdcall get_latencies(int32_t *input_latency, int32_t *output_latency) = 0;
        virtual asio_error __stdcall get_buffer_size(
            int32_t *min_size,
            int32_t *max_size,
            int32_t *preferred_size,
            int32_t *granularity) = 0;
        virtual asio_error __stdcall can_sample_rate(asio_sample_rate sample_rate) = 0;
        virtual asio_error __stdcall get_sample_rate(asio_sample_rate *sample_rate) = 0;
        virtual asio_error __stdcall set_sample_rate(asio_sample_rate sample_rate) = 0;

        virtual asio_error __stdcall get_clock_sources(void *clocks, int32_t *num_sources) = 0;
        virtual asio_error __stdcall set_clock_source(int32_t reference) = 0;
        virtual asio_error __stdcall get_sample_position(void *sample_position, void *time_stamp) = 0;
        virtual asio_error __stdcall get_channel_info(asio_channel_info *info) = 0;
        virtual asio_error __stdcall create_buffers(
            asio_buffer_info *buffer_infos,
            int32_t num_channels,
            int32_t buffer_size,
            asio_callbacks *callbacks) = 0;
        virtual asio_error __stdcall dispose_buffers() = 0;
        virtual asio_error __stdcall control_panel() = 0;
        virtual asio_error __stdcall future(int32_t selector, void *opt) = 0;
        virtual asio_error __stdcall output_ready() = 0;
    };

    // Standard IID for IUnknown: 00000000-0000-0000-C000-000000000046
    inline constexpr GUID iid_iunknown =
        {
            0x00000000,
            0x0000,
            0x0000,
            {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46},
    };

    struct installed_driver
    {
        std::wstring name;
        std::wstring dll_path;
        GUID clsid{};
    };

    // Enumerates drivers from the common registry locations.
    // This avoids COM/CoCreateInstance but still uses the conventional ASIO registry keys.
    std::vector<installed_driver> enumerate_installed_drivers();

    std::optional<installed_driver> find_installed_driver(std::wstring_view driver_name);

    // Parses "{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}" or "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx".
    std::optional<GUID> try_parse_guid(std::wstring_view text);

    class driver
    {
    public:
        driver() = default;
        ~driver();

        driver(const driver &) = delete;
        driver &operator=(const driver &) = delete;

        driver(driver &&other) noexcept;
        driver &operator=(driver &&other) noexcept;

        void load_library(const std::wstring &dll_path);
        void create_instance(const GUID &clsid);
        void unload();

        bool is_loaded() const noexcept { return module_ != nullptr; }
        bool has_instance() const noexcept { return asio_ != nullptr; }

        iasio *get() noexcept { return asio_; }
        const iasio *get() const noexcept { return asio_; }

    private:
        using dll_get_class_object_fn = hresult(WINAPI *)(const GUID &rclsid, const GUID &riid, void **ppv);

        void reset_instance() noexcept;
        void reset_library() noexcept;

        HMODULE module_ = nullptr;
        dll_get_class_object_fn dll_get_class_object_ = nullptr;
        iasio *asio_ = nullptr;
    };

} // namespace catalyst::audio::asio
