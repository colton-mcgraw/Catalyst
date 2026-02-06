#pragma once

#include <catalyst/math/simd.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace catalyst::math::detail
{
    template <typename T>
    struct simd_pack
    {
        static constexpr bool available = false;
    };

    template <>
    struct simd_pack<float>
    {
        static constexpr bool available = true;
        using type = f32x4;
        static constexpr std::size_t lanes = 4;

        static type load(const float* p) noexcept { return type::load_unaligned(p); }
        static void store(float* p, type v) noexcept { v.store_unaligned(p); }
    };

    template <>
    struct simd_pack<double>
    {
        static constexpr bool available = true;
        using type = f64x2;
        static constexpr std::size_t lanes = 2;

        static type load(const double* p) noexcept { return type::load_unaligned(p); }
        static void store(double* p, type v) noexcept { v.store_unaligned(p); }
    };

    template <>
    struct simd_pack<std::int32_t>
    {
        static constexpr bool available = true;
        using type = i32x4;
        static constexpr std::size_t lanes = 4;

        static type load(const std::int32_t* p) noexcept { return type::load_unaligned(p); }
        static void store(std::int32_t* p, type v) noexcept { v.store_unaligned(p); }
    };

    template <>
    struct simd_pack<std::uint32_t>
    {
        static constexpr bool available = true;
        using type = u32x4;
        static constexpr std::size_t lanes = 4;

        static type load(const std::uint32_t* p) noexcept { return type::load_unaligned(p); }
        static void store(std::uint32_t* p, type v) noexcept { v.store_unaligned(p); }
    };

    template <>
    struct simd_pack<std::int64_t>
    {
        static constexpr bool available = true;
        using type = i64x2;
        static constexpr std::size_t lanes = 2;

        static type load(const std::int64_t* p) noexcept { return type::load_unaligned(p); }
        static void store(std::int64_t* p, type v) noexcept { v.store_unaligned(p); }
    };

    template <>
    struct simd_pack<std::uint64_t>
    {
        static constexpr bool available = true;
        using type = u64x2;
        static constexpr std::size_t lanes = 2;

        static type load(const std::uint64_t* p) noexcept { return type::load_unaligned(p); }
        static void store(std::uint64_t* p, type v) noexcept { v.store_unaligned(p); }
    };

    template <typename T>
    inline constexpr bool has_simd_pack_v = simd_pack<T>::available;

    template <typename T, std::size_t N>
    inline constexpr bool simd_compatible_v = has_simd_pack_v<T> && (N % simd_pack<T>::lanes == 0);

    template <typename T, std::size_t N>
    inline constexpr void fill(T* dst, T value) noexcept
    {
        for (std::size_t i = 0; i < N; ++i)
            dst[i] = value;
    }

    template <typename T, std::size_t N>
    inline constexpr void copy(T* dst, const T* src) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < N; ++i)
                dst[i] = src[i];
            return;
        }

        if constexpr (simd_compatible_v<T, N>)
        {
            using pack = simd_pack<T>;
            for (std::size_t i = 0; i < N; i += pack::lanes)
            {
                const auto chunk = pack::load(src + i);
                pack::store(dst + i, chunk);
            }
        }
        else
        {
            for (std::size_t i = 0; i < N; ++i)
                dst[i] = src[i];
        }
    }

    template <typename T, std::size_t N>
    inline constexpr void add_inplace(T* a, const T* b) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < N; ++i)
                a[i] += b[i];
            return;
        }

        if constexpr (simd_compatible_v<T, N>)
        {
            using pack = simd_pack<T>;
            using simd_t = typename pack::type;
            for (std::size_t i = 0; i < N; i += pack::lanes)
            {
                const simd_t av = pack::load(a + i);
                const simd_t bv = pack::load(b + i);
                pack::store(a + i, av + bv);
            }
        }
        else
        {
            for (std::size_t i = 0; i < N; ++i)
                a[i] += b[i];
        }
    }

    template <typename T, std::size_t N>
    inline constexpr void sub_inplace(T* a, const T* b) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < N; ++i)
                a[i] -= b[i];
            return;
        }

        if constexpr (simd_compatible_v<T, N>)
        {
            using pack = simd_pack<T>;
            using simd_t = typename pack::type;
            for (std::size_t i = 0; i < N; i += pack::lanes)
            {
                const simd_t av = pack::load(a + i);
                const simd_t bv = pack::load(b + i);
                pack::store(a + i, av - bv);
            }
        }
        else
        {
            for (std::size_t i = 0; i < N; ++i)
                a[i] -= b[i];
        }
    }

    template <typename T, std::size_t N>
    inline constexpr void mul_inplace(T* a, const T* b) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < N; ++i)
                a[i] *= b[i];
            return;
        }

        if constexpr (simd_compatible_v<T, N> && (std::is_same_v<T, float> || std::is_same_v<T, double>))
        {
            using pack = simd_pack<T>;
            using simd_t = typename pack::type;
            for (std::size_t i = 0; i < N; i += pack::lanes)
            {
                const simd_t av = pack::load(a + i);
                const simd_t bv = pack::load(b + i);
                pack::store(a + i, av * bv);
            }
        }
        else
        {
            for (std::size_t i = 0; i < N; ++i)
                a[i] *= b[i];
        }
    }

    template <typename T, std::size_t N>
    inline constexpr void div_inplace(T* a, const T* b) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < N; ++i)
                a[i] /= b[i];
            return;
        }

        if constexpr (simd_compatible_v<T, N> && (std::is_same_v<T, float> || std::is_same_v<T, double>))
        {
            using pack = simd_pack<T>;
            using simd_t = typename pack::type;
            for (std::size_t i = 0; i < N; i += pack::lanes)
            {
                const simd_t av = pack::load(a + i);
                const simd_t bv = pack::load(b + i);
                pack::store(a + i, av / bv);
            }
        }
        else
        {
            for (std::size_t i = 0; i < N; ++i)
                a[i] /= b[i];
        }
    }

    template <typename T, std::size_t N>
    [[nodiscard]] inline constexpr T dot(const T* a, const T* b) noexcept
    {
        if (std::is_constant_evaluated())
        {
            T sum{};
            for (std::size_t i = 0; i < N; ++i)
                sum += a[i] * b[i];
            return sum;
        }

        if constexpr (simd_compatible_v<T, N> && std::is_same_v<T, float>)
        {
            using pack = simd_pack<T>;
            using simd_t = typename pack::type;
            simd_t acc = simd_t::zero();
            for (std::size_t i = 0; i < N; i += pack::lanes)
            {
                const simd_t av = pack::load(a + i);
                const simd_t bv = pack::load(b + i);
                acc += (av * bv);
            }
            alignas(16) float tmp[4]{};
            acc.store_unaligned(tmp);
            float sum = tmp[0] + tmp[1] + tmp[2] + tmp[3];
            return static_cast<T>(sum);
        }
        else if constexpr (simd_compatible_v<T, N> && std::is_same_v<T, double>)
        {
            using pack = simd_pack<T>;
            using simd_t = typename pack::type;
            simd_t acc = simd_t::zero();
            for (std::size_t i = 0; i < N; i += pack::lanes)
            {
                const simd_t av = pack::load(a + i);
                const simd_t bv = pack::load(b + i);
                acc += (av * bv);
            }
            alignas(16) double tmp[2]{};
            acc.store_unaligned(tmp);
            double sum = tmp[0] + tmp[1];
            return static_cast<T>(sum);
        }
        else
        {
            T sum{};
            for (std::size_t i = 0; i < N; ++i)
                sum += a[i] * b[i];
            return sum;
        }
    }

    template <typename T, std::size_t N>
    inline constexpr void min_to(T* out, const T* a, const T* b) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < N; ++i)
                out[i] = (std::min)(a[i], b[i]);
            return;
        }

        if constexpr (simd_compatible_v<T, N> && (std::is_same_v<T, float> || std::is_same_v<T, double>))
        {
            using pack = simd_pack<T>;
            using simd_t = typename pack::type;
            for (std::size_t i = 0; i < N; i += pack::lanes)
            {
                const simd_t av = pack::load(a + i);
                const simd_t bv = pack::load(b + i);
                pack::store(out + i, av.min(bv));
            }
        }
        else
        {
            for (std::size_t i = 0; i < N; ++i)
                out[i] = (std::min)(a[i], b[i]);
        }
    }

    template <typename T, std::size_t N>
    inline constexpr void max_to(T* out, const T* a, const T* b) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < N; ++i)
                out[i] = (std::max)(a[i], b[i]);
            return;
        }

        if constexpr (simd_compatible_v<T, N> && (std::is_same_v<T, float> || std::is_same_v<T, double>))
        {
            using pack = simd_pack<T>;
            using simd_t = typename pack::type;
            for (std::size_t i = 0; i < N; i += pack::lanes)
            {
                const simd_t av = pack::load(a + i);
                const simd_t bv = pack::load(b + i);
                pack::store(out + i, av.max(bv));
            }
        }
        else
        {
            for (std::size_t i = 0; i < N; ++i)
                out[i] = (std::max)(a[i], b[i]);
        }
    }

} // namespace catalyst::math::detail
