#include "asio_loader.h"

#include <algorithm>
#include <stdexcept>

namespace catalyst::audio::asio {

namespace {

constexpr bool is_hex(wchar_t c)
{
    return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F');
}

constexpr uint8_t hex_to_u8(wchar_t c)
{
    if (c >= L'0' && c <= L'9')
        return static_cast<uint8_t>(c - L'0');
    if (c >= L'a' && c <= L'f')
        return static_cast<uint8_t>(10 + (c - L'a'));
    return static_cast<uint8_t>(10 + (c - L'A'));
}

std::optional<uint32_t> parse_hex_u32(std::wstring_view s)
{
    if (s.empty() || s.size() > 8)
        return std::nullopt;
    uint32_t out = 0;
    for (auto ch : s)
    {
        if (!is_hex(ch))
            return std::nullopt;
        out = (out << 4) | hex_to_u8(ch);
    }
    return out;
}

std::optional<uint16_t> parse_hex_u16(std::wstring_view s)
{
    if (s.empty() || s.size() > 4)
        return std::nullopt;
    uint32_t tmp = 0;
    for (auto ch : s)
    {
        if (!is_hex(ch))
            return std::nullopt;
        tmp = (tmp << 4) | hex_to_u8(ch);
    }
    return static_cast<uint16_t>(tmp);
}

std::optional<uint8_t> parse_hex_u8(std::wstring_view s)
{
    if (s.size() != 2)
        return std::nullopt;
    if (!is_hex(s[0]) || !is_hex(s[1]))
        return std::nullopt;
    return static_cast<uint8_t>((hex_to_u8(s[0]) << 4) | hex_to_u8(s[1]));
}

std::wstring read_reg_string(HKEY root, const wchar_t* subkey, const wchar_t* value)
{
    DWORD type = 0;
    DWORD bytes = 0;
    auto rc = RegGetValueW(root, subkey, value, RRF_RT_REG_SZ, &type, nullptr, &bytes);
    if (rc != ERROR_SUCCESS || bytes == 0)
        return {};

    std::wstring out;
    out.resize(bytes / sizeof(wchar_t));
    rc = RegGetValueW(root, subkey, value, RRF_RT_REG_SZ, &type, out.data(), &bytes);
    if (rc != ERROR_SUCCESS)
        return {};

    // Ensure null termination is trimmed.
    if (!out.empty() && out.back() == L'\0')
        out.pop_back();
    return out;
}

void enumerate_asio_key(HKEY root, const wchar_t* key_path, std::vector<installed_driver>& out)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, key_path, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return;

    DWORD index = 0;
    wchar_t name_buf[256] = {};
    DWORD name_len = static_cast<DWORD>(std::size(name_buf));

    while (true)
    {
        name_len = static_cast<DWORD>(std::size(name_buf));
        auto rc = RegEnumKeyExW(key, index, name_buf, &name_len, nullptr, nullptr, nullptr, nullptr);
        if (rc == ERROR_NO_MORE_ITEMS)
            break;
        if (rc != ERROR_SUCCESS)
        {
            ++index;
            continue;
        }

        std::wstring subkey = std::wstring(key_path) + L"\\" + std::wstring(name_buf, name_len);

        installed_driver driver;
        driver.name = std::wstring(name_buf, name_len);
        driver.dll_path = read_reg_string(root, subkey.c_str(), L"DLL");

        auto clsid_text = read_reg_string(root, subkey.c_str(), L"CLSID");
        auto clsid = try_parse_guid(clsid_text);
        if (!driver.dll_path.empty() && clsid.has_value())
        {
            driver.clsid = *clsid;
            out.push_back(std::move(driver));
        }

        ++index;
    }

    RegCloseKey(key);
}

} // namespace

std::optional<GUID> try_parse_guid(std::wstring_view text)
{
    // Accept optional braces.
    if (!text.empty() && text.front() == L'{')
    {
        if (text.size() < 2 || text.back() != L'}')
            return std::nullopt;
        text = text.substr(1, text.size() - 2);
    }

    // Format: 8-4-4-4-12
    //         0        1 2 3 4
    auto dash1 = text.find(L'-');
    if (dash1 == std::wstring_view::npos)
        return std::nullopt;
    auto dash2 = text.find(L'-', dash1 + 1);
    auto dash3 = text.find(L'-', dash2 + 1);
    auto dash4 = text.find(L'-', dash3 + 1);
    if (dash2 == std::wstring_view::npos || dash3 == std::wstring_view::npos || dash4 == std::wstring_view::npos)
        return std::nullopt;

    auto p0 = text.substr(0, dash1);
    auto p1 = text.substr(dash1 + 1, dash2 - (dash1 + 1));
    auto p2 = text.substr(dash2 + 1, dash3 - (dash2 + 1));
    auto p3 = text.substr(dash3 + 1, dash4 - (dash3 + 1));
    auto p4 = text.substr(dash4 + 1);

    if (p0.size() != 8 || p1.size() != 4 || p2.size() != 4 || p3.size() != 4 || p4.size() != 12)
        return std::nullopt;

    auto d1 = parse_hex_u32(p0);
    auto d2 = parse_hex_u16(p1);
    auto d3 = parse_hex_u16(p2);
    if (!d1 || !d2 || !d3)
        return std::nullopt;

    // p3 => 2 bytes, p4 => 6 bytes
    auto b0 = parse_hex_u8(p3.substr(0, 2));
    auto b1 = parse_hex_u8(p3.substr(2, 2));
    if (!b0 || !b1)
        return std::nullopt;

    uint8_t b_rest[6] = {};
    for (size_t i = 0; i < 6; ++i)
    {
        auto byte = parse_hex_u8(p4.substr(i * 2, 2));
        if (!byte)
            return std::nullopt;
        b_rest[i] = *byte;
    }

    GUID g{};
    g.Data1 = *d1;
    g.Data2 = *d2;
    g.Data3 = *d3;
    g.Data4[0] = *b0;
    g.Data4[1] = *b1;
    for (size_t i = 0; i < 6; ++i)
        g.Data4[2 + i] = b_rest[i];
    return g;
}

std::vector<installed_driver> enumerate_installed_drivers()
{
    std::vector<installed_driver> out;
    enumerate_asio_key(HKEY_LOCAL_MACHINE, L"SOFTWARE\\ASIO", out);
    enumerate_asio_key(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\ASIO", out);
    return out;
}

std::optional<installed_driver> find_installed_driver(std::wstring_view driver_name)
{
    auto drivers = enumerate_installed_drivers();
    auto it = std::find_if(drivers.begin(), drivers.end(), [&](const installed_driver& d) {
        return d.name == driver_name;
    });
    if (it == drivers.end())
        return std::nullopt;
    return *it;
}

driver::~driver()
{
    unload();
}

driver::driver(driver&& other) noexcept
{
    module_ = other.module_;
    dll_get_class_object_ = other.dll_get_class_object_;
    asio_ = other.asio_;

    other.module_ = nullptr;
    other.dll_get_class_object_ = nullptr;
    other.asio_ = nullptr;
}

driver& driver::operator=(driver&& other) noexcept
{
    if (this != &other)
    {
        unload();

        module_ = other.module_;
        dll_get_class_object_ = other.dll_get_class_object_;
        asio_ = other.asio_;

        other.module_ = nullptr;
        other.dll_get_class_object_ = nullptr;
        other.asio_ = nullptr;
    }
    return *this;
}

void driver::reset_instance() noexcept
{
    if (asio_)
    {
        asio_->Release();
        asio_ = nullptr;
    }
}

void driver::reset_library() noexcept
{
    if (module_)
    {
        FreeLibrary(module_);
        module_ = nullptr;
    }
    dll_get_class_object_ = nullptr;
}

void driver::unload()
{
    reset_instance();
    reset_library();
}

void driver::load_library(const std::wstring& dll_path)
{
    unload();

    module_ = LoadLibraryW(dll_path.c_str());
    if (!module_)
        throw std::runtime_error("Failed to load ASIO driver DLL");

    auto proc = GetProcAddress(module_, "DllGetClassObject");
    if (!proc)
    {
        reset_library();
        throw std::runtime_error("ASIO driver DLL does not export DllGetClassObject");
    }

    dll_get_class_object_ = reinterpret_cast<dll_get_class_object_fn>(proc);
}

void driver::create_instance(const GUID& clsid)
{
    if (!module_ || !dll_get_class_object_)
        throw std::runtime_error("ASIO driver library is not loaded");

    reset_instance();

    iclass_factory* factory = nullptr;
    const auto hr = dll_get_class_object_(clsid, iid_iunknown, reinterpret_cast<void**>(&factory));
    if (hr < 0 || !factory)
        throw std::runtime_error("DllGetClassObject failed");

    void* unknown_ptr = nullptr;
    const auto hr2 = factory->CreateInstance(nullptr, iid_iunknown, &unknown_ptr);
    factory->Release();

    if (hr2 < 0 || !unknown_ptr)
        throw std::runtime_error("IClassFactory::CreateInstance failed");

    // Many ASIO drivers return their primary IASIO pointer for IID_IUnknown.
    asio_ = reinterpret_cast<iasio*>(unknown_ptr);
}

} // namespace catalyst::audio::asio