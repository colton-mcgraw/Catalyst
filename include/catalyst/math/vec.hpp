#pragma once

#include <catalyst/math/simd.hpp>
#include <catalyst/math/detail/simd_array_ops.hpp>

#include <array>
#include <algorithm>
#include <concepts>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <type_traits>

namespace catalyst::math
{

    template <typename T>
    inline constexpr bool is_vec_scalar_v = std::is_arithmetic_v<T>;

    template <typename T, std::size_t N, std::size_t Align, typename Enable>
    struct vec;

    namespace detail
    {
        template <typename T, std::size_t M>
        using swizzle_vec_t = ::catalyst::math::vec<T, M, alignof(T), std::enable_if_t<is_vec_scalar_v<T>>>;

        template <typename Derived, typename T, std::size_t N>
        struct vec_common
        {
            using value_type = T;
            static constexpr std::size_t size = N;

            [[nodiscard]] constexpr T *data() noexcept { return static_cast<Derived *>(this)->data(); }
            [[nodiscard]] constexpr const T *data() const noexcept { return static_cast<const Derived *>(this)->data(); }

            [[nodiscard]] constexpr T &operator[](std::size_t i) noexcept { return data()[i]; }
            [[nodiscard]] constexpr const T &operator[](std::size_t i) const noexcept { return data()[i]; }

            [[nodiscard]] static constexpr Derived load(const T *ptr) noexcept
            {
                Derived out;
                detail::copy<T, N>(out.data(), ptr);
                return out;
            }

            constexpr void store(T *ptr) const noexcept
            {
                detail::copy<T, N>(ptr, data());
            }

            [[nodiscard]] constexpr Derived operator+() const noexcept { return *static_cast<const Derived *>(this); }

            [[nodiscard]] constexpr Derived operator-() const noexcept
            {
                Derived out;
                for (std::size_t i = 0; i < N; ++i)
                    out.data()[i] = -data()[i];
                return out;
            }

            [[nodiscard]] friend constexpr Derived operator+(Derived a, const Derived &b) noexcept
            {
                detail::add_inplace<T, N>(a.data(), b.data());
                return a;
            }

            [[nodiscard]] friend constexpr Derived operator-(Derived a, const Derived &b) noexcept
            {
                detail::sub_inplace<T, N>(a.data(), b.data());
                return a;
            }

            [[nodiscard]] friend constexpr Derived operator*(Derived a, const Derived &b) noexcept
            {
                detail::mul_inplace<T, N>(a.data(), b.data());
                return a;
            }

            [[nodiscard]] friend constexpr Derived operator/(Derived a, const Derived &b) noexcept
            {
                detail::div_inplace<T, N>(a.data(), b.data());
                return a;
            }

            [[nodiscard]] friend constexpr Derived operator+(Derived a, T s) noexcept
            {
                for (std::size_t i = 0; i < N; ++i)
                    a.data()[i] += s;
                return a;
            }

            [[nodiscard]] friend constexpr Derived operator-(Derived a, T s) noexcept
            {
                for (std::size_t i = 0; i < N; ++i)
                    a.data()[i] -= s;
                return a;
            }

            [[nodiscard]] friend constexpr Derived operator*(Derived a, T s) noexcept
            {
                for (std::size_t i = 0; i < N; ++i)
                    a.data()[i] *= s;
                return a;
            }

            [[nodiscard]] friend constexpr Derived operator/(Derived a, T s) noexcept
            {
                for (std::size_t i = 0; i < N; ++i)
                    a.data()[i] /= s;
                return a;
            }

            [[nodiscard]] friend constexpr Derived operator+(T s, Derived a) noexcept { return a + s; }
            [[nodiscard]] friend constexpr Derived operator*(T s, Derived a) noexcept { return a * s; }

            constexpr Derived &operator+=(const Derived &other) noexcept { return *static_cast<Derived *>(this) = (*static_cast<Derived *>(this) + other); }
            constexpr Derived &operator-=(const Derived &other) noexcept { return *static_cast<Derived *>(this) = (*static_cast<Derived *>(this) - other); }
            constexpr Derived &operator*=(const Derived &other) noexcept { return *static_cast<Derived *>(this) = (*static_cast<Derived *>(this) * other); }
            constexpr Derived &operator/=(const Derived &other) noexcept { return *static_cast<Derived *>(this) = (*static_cast<Derived *>(this) / other); }

            constexpr Derived &operator+=(T s) noexcept { return *static_cast<Derived *>(this) = (*static_cast<Derived *>(this) + s); }
            constexpr Derived &operator-=(T s) noexcept { return *static_cast<Derived *>(this) = (*static_cast<Derived *>(this) - s); }
            constexpr Derived &operator*=(T s) noexcept { return *static_cast<Derived *>(this) = (*static_cast<Derived *>(this) * s); }
            constexpr Derived &operator/=(T s) noexcept { return *static_cast<Derived *>(this) = (*static_cast<Derived *>(this) / s); }

            [[nodiscard]] friend constexpr bool operator==(const Derived &a, const Derived &b) noexcept
            {
                for (std::size_t i = 0; i < N; ++i)
                {
                    if (!(a.data()[i] == b.data()[i]))
                        return false;
                }
                return true;
            }

            [[nodiscard]] friend constexpr bool operator!=(const Derived &a, const Derived &b) noexcept { return !(a == b); }

            [[nodiscard]] constexpr T dot(const Derived &other) const noexcept
            {
                return detail::dot<T, N>(data(), other.data());
            }

            template <typename U = T>
            requires std::floating_point<U>
            [[nodiscard]] U length_sq() const noexcept
            {
                return static_cast<U>(this->dot(*static_cast<const Derived *>(this)));
            }

            template <typename U = T>
            requires std::floating_point<U>
            [[nodiscard]] U length() const noexcept
            {
                return static_cast<U>(std::sqrt(this->length_sq()));
            }

            template <typename U = T>
            requires std::floating_point<U>
            [[nodiscard]] Derived normalized() const noexcept
            {
                const U len = this->length();
                if (len == U{})
                    return Derived{};
                return (*static_cast<const Derived *>(this)) / static_cast<T>(len);
            }

            [[nodiscard]] constexpr Derived min(const Derived &other) const noexcept
            {
                Derived out;
                detail::min_to<T, N>(out.data(), data(), other.data());
                return out;
            }

            [[nodiscard]] constexpr Derived max(const Derived &other) const noexcept
            {
                Derived out;
                detail::max_to<T, N>(out.data(), data(), other.data());
                return out;
            }

            [[nodiscard]] constexpr Derived clamp(const Derived &lo, const Derived &hi) const noexcept
            {
                return this->max(lo).min(hi);
            }

            template <std::size_t... I>
                requires (sizeof...(I) > 0) && ((I < N) && ...)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, sizeof...(I)> swizzle() const noexcept
            {
                return detail::swizzle_vec_t<T, sizeof...(I)>{data()[I]...};
            }

            // Common named swizzles.
            template <std::size_t M = N>
                requires (M >= 2)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 2> xx() const noexcept
            {
                return this->template swizzle<0, 0>();
            }

            template <std::size_t M = N>
                requires (M >= 2)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 2> xy() const noexcept
            {
                return this->template swizzle<0, 1>();
            }

            template <std::size_t M = N>
                requires (M >= 2)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 2> yx() const noexcept
            {
                return this->template swizzle<1, 0>();
            }

            template <std::size_t M = N>
                requires (M >= 2)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 2> yy() const noexcept
            {
                return this->template swizzle<1, 1>();
            }

            template <std::size_t M = N>
                requires (M >= 3)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 2> xz() const noexcept
            {
                return this->template swizzle<0, 2>();
            }

            template <std::size_t M = N>
                requires (M >= 3)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 2> yz() const noexcept
            {
                return this->template swizzle<1, 2>();
            }

            template <std::size_t M = N>
                requires (M >= 3)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 2> zx() const noexcept
            {
                return this->template swizzle<2, 0>();
            }

            template <std::size_t M = N>
                requires (M >= 3)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 2> zy() const noexcept
            {
                return this->template swizzle<2, 1>();
            }

            template <std::size_t M = N>
                requires (M >= 3)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 3> xyz() const noexcept
            {
                return this->template swizzle<0, 1, 2>();
            }

            template <std::size_t M = N>
                requires (M >= 3)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 3> xzy() const noexcept
            {
                return this->template swizzle<0, 2, 1>();
            }

            template <std::size_t M = N>
                requires (M >= 3)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 3> yxz() const noexcept
            {
                return this->template swizzle<1, 0, 2>();
            }

            template <std::size_t M = N>
                requires (M >= 3)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 3> yzx() const noexcept
            {
                return this->template swizzle<1, 2, 0>();
            }

            template <std::size_t M = N>
                requires (M >= 3)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 3> zxy() const noexcept
            {
                return this->template swizzle<2, 0, 1>();
            }

            template <std::size_t M = N>
                requires (M >= 3)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 3> zyx() const noexcept
            {
                return this->template swizzle<2, 1, 0>();
            }

            template <std::size_t M = N>
                requires (M >= 4)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 2> zw() const noexcept
            {
                return this->template swizzle<2, 3>();
            }

            template <std::size_t M = N>
                requires (M >= 4)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 4> xyzw() const noexcept
            {
                return this->template swizzle<0, 1, 2, 3>();
            }

            template <std::size_t M = N>
                requires (M >= 4)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 4> wzyx() const noexcept
            {
                return this->template swizzle<3, 2, 1, 0>();
            }
        };
    }

    template <typename T, std::size_t N, std::size_t Align = alignof(T), typename = std::enable_if_t<is_vec_scalar_v<T>>>
    struct alignas(Align) vec : detail::vec_common<vec<T, N, Align, std::enable_if_t<is_vec_scalar_v<T>>>, T, N>
    {
        static_assert(N > 0, "vec<T,N>: N must be > 0");

        std::array<T, N> v{};

        constexpr vec() noexcept = default;

        explicit constexpr vec(T splat_value) noexcept
        {
            detail::fill<T, N>(data(), splat_value);
        }

        constexpr vec(std::initializer_list<T> init) noexcept
        {
            const std::size_t count = (std::min)(init.size(), N);
            auto it = init.begin();
            for (std::size_t i = 0; i < count; ++i, ++it)
                data()[i] = *it;
            for (std::size_t i = count; i < N; ++i)
                data()[i] = T{};
        }

        [[nodiscard]] constexpr T *data() noexcept { return v.data(); }
        [[nodiscard]] constexpr const T *data() const noexcept { return v.data(); }
    };

    template <typename T, std::size_t Align>
    struct alignas(Align) vec<T, 2, Align, std::enable_if_t<is_vec_scalar_v<T>>> : detail::vec_common<vec<T, 2, Align, std::enable_if_t<is_vec_scalar_v<T>>>, T, 2>
    {
        using value_type = T;
        T x{};
        T y{};

        constexpr vec() noexcept = default;
        constexpr vec(T x_, T y_) noexcept : x(x_), y(y_) {}

        explicit constexpr vec(T splat_value) noexcept : x(splat_value), y(splat_value) {}

        constexpr vec(std::initializer_list<T> init) noexcept
        {
            const std::size_t count = (std::min)(init.size(), std::size_t{2});
            auto it = init.begin();
            for (std::size_t i = 0; i < count; ++i, ++it)
                data()[i] = *it;
            for (std::size_t i = count; i < 2; ++i)
                data()[i] = T{};
        }

        [[nodiscard]] constexpr T *data() noexcept { return &x; }
        [[nodiscard]] constexpr const T *data() const noexcept { return &x; }
    };

    template <typename T, std::size_t Align>
    struct alignas(Align) vec<T, 3, Align, std::enable_if_t<is_vec_scalar_v<T>>> : detail::vec_common<vec<T, 3, Align, std::enable_if_t<is_vec_scalar_v<T>>>, T, 3>
    {
        using value_type = T;
        T x{};
        T y{};
        T z{};

        constexpr vec() noexcept = default;
        constexpr vec(T x_, T y_, T z_) noexcept : x(x_), y(y_), z(z_) {}

        explicit constexpr vec(T splat_value) noexcept : x(splat_value), y(splat_value), z(splat_value) {}

        constexpr vec(std::initializer_list<T> init) noexcept
        {
            const std::size_t count = (std::min)(init.size(), std::size_t{3});
            auto it = init.begin();
            for (std::size_t i = 0; i < count; ++i, ++it)
                data()[i] = *it;
            for (std::size_t i = count; i < 3; ++i)
                data()[i] = T{};
        }

        [[nodiscard]] constexpr T *data() noexcept { return &x; }
        [[nodiscard]] constexpr const T *data() const noexcept { return &x; }

        [[nodiscard]] constexpr vec cross(const vec &other) const noexcept
        {
            return vec{
                y * other.z - z * other.y,
                z * other.x - x * other.z,
                x * other.y - y * other.x,
            };
        }
    };

    template <typename T, std::size_t Align>
    struct alignas(Align) vec<T, 4, Align, std::enable_if_t<is_vec_scalar_v<T>>> : detail::vec_common<vec<T, 4, Align, std::enable_if_t<is_vec_scalar_v<T>>>, T, 4>
    {
        using value_type = T;
        T x{};
        T y{};
        T z{};
        T w{};

        constexpr vec() noexcept = default;
        constexpr vec(T x_, T y_, T z_, T w_) noexcept : x(x_), y(y_), z(z_), w(w_) {}

        explicit constexpr vec(T splat_value) noexcept : x(splat_value), y(splat_value), z(splat_value), w(splat_value) {}

        constexpr vec(std::initializer_list<T> init) noexcept
        {
            const std::size_t count = (std::min)(init.size(), std::size_t{4});
            auto it = init.begin();
            for (std::size_t i = 0; i < count; ++i, ++it)
                data()[i] = *it;
            for (std::size_t i = count; i < 4; ++i)
                data()[i] = T{};
        }

        [[nodiscard]] constexpr T *data() noexcept { return &x; }
        [[nodiscard]] constexpr const T *data() const noexcept { return &x; }
    };

    template <typename T>
    using vec2 = vec<T, 2>;
    template <typename T>
    using vec3 = vec<T, 3>;
    template <typename T>
    using vec4 = vec<T, 4>;

    using vec2f = vec2<float>;
    using vec3f = vec3<float>;
    using vec4f = vec4<float>;

    using vec2d = vec2<double>;
    using vec3d = vec3<double>;
    using vec4d = vec4<double>;

} // namespace catalyst::math
