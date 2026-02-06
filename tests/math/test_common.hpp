#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string_view>

namespace catalyst::tests
{

[[noreturn]] inline void fail(std::string_view expr, std::string_view file, int line)
{
    std::cerr << "TEST FAILED: " << expr << " (" << file << ":" << line << ")\n";
    std::exit(1);
}

#define CT_REQUIRE(expr) \
    do                 \
    {                  \
        if (!(expr))   \
        {              \
            ::catalyst::tests::fail(#expr, __FILE__, __LINE__); \
        }              \
    } while (false)

template <class T>
inline bool nearly_equal(T a, T b, T eps = static_cast<T>(1e-5)) noexcept
{
    const T diff = std::fabs(a - b);
    if (diff <= eps)
        return true;

    const T scale = (std::max)(std::fabs(a), std::fabs(b));
    return diff <= eps * (std::max)(static_cast<T>(1), scale);
}

template <class Vec4>
inline std::array<float, 4> lanes(const Vec4 &v)
{
    std::array<float, 4> out{};
    v.store_unaligned(out.data());
    return out;
}

template <class Vec4>
inline std::array<std::uint32_t, 4> lane_bits(const Vec4 &v)
{
    const auto f = lanes(v);
    std::array<std::uint32_t, 4> bits{};

#if defined(__cpp_lib_bit_cast) && __cpp_lib_bit_cast >= 201806L
    for (std::size_t i = 0; i < 4; ++i)
    {
        bits[i] = std::bit_cast<std::uint32_t>(f[i]);
    }
#else
    for (std::size_t i = 0; i < 4; ++i)
    {
        std::memcpy(&bits[i], &f[i], sizeof(bits[i]));
    }
#endif

    return bits;
}

} // namespace catalyst::tests
