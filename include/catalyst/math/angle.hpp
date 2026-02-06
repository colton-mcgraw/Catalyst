#pragma once

#include <compare>
#include <concepts>
#include <numbers>

namespace catalyst::math
{

    template <std::floating_point T>
    struct radians
    {
        using value_type = T;

        T value{};

        constexpr radians() noexcept = default;
        constexpr explicit radians(T v) noexcept : value(v) {}

        [[nodiscard]] constexpr T count() const noexcept { return value; }

        constexpr auto operator<=>(const radians &) const = default;

        [[nodiscard]] friend constexpr radians operator+(radians a, radians b) noexcept { return radians{a.value + b.value}; }
        [[nodiscard]] friend constexpr radians operator-(radians a, radians b) noexcept { return radians{a.value - b.value}; }

        constexpr radians &operator+=(radians other) noexcept
        {
            value += other.value;
            return *this;
        }
        constexpr radians &operator-=(radians other) noexcept
        {
            value -= other.value;
            return *this;
        }

        [[nodiscard]] friend constexpr radians operator*(radians a, T s) noexcept { return radians{a.value * s}; }
        [[nodiscard]] friend constexpr radians operator*(T s, radians a) noexcept { return radians{a.value * s}; }
        [[nodiscard]] friend constexpr radians operator/(radians a, T s) noexcept { return radians{a.value / s}; }

        constexpr radians &operator*=(T s) noexcept
        {
            value *= s;
            return *this;
        }
        constexpr radians &operator/=(T s) noexcept
        {
            value /= s;
            return *this;
        }
    };

    template <std::floating_point T>
    struct degrees
    {
        using value_type = T;

        T value{};

        constexpr degrees() noexcept = default;
        constexpr explicit degrees(T v) noexcept : value(v) {}

        [[nodiscard]] constexpr T count() const noexcept { return value; }

        constexpr auto operator<=>(const degrees &) const = default;

        [[nodiscard]] friend constexpr degrees operator+(degrees a, degrees b) noexcept { return degrees{a.value + b.value}; }
        [[nodiscard]] friend constexpr degrees operator-(degrees a, degrees b) noexcept { return degrees{a.value - b.value}; }

        constexpr degrees &operator+=(degrees other) noexcept
        {
            value += other.value;
            return *this;
        }
        constexpr degrees &operator-=(degrees other) noexcept
        {
            value -= other.value;
            return *this;
        }

        [[nodiscard]] friend constexpr degrees operator*(degrees a, T s) noexcept { return degrees{a.value * s}; }
        [[nodiscard]] friend constexpr degrees operator*(T s, degrees a) noexcept { return degrees{a.value * s}; }
        [[nodiscard]] friend constexpr degrees operator/(degrees a, T s) noexcept { return degrees{a.value / s}; }

        constexpr degrees &operator*=(T s) noexcept
        {
            value *= s;
            return *this;
        }
        constexpr degrees &operator/=(T s) noexcept
        {
            value /= s;
            return *this;
        }
    };

    template <std::floating_point T>
    [[nodiscard]] constexpr radians<T> rad(T v) noexcept
    {
        return radians<T>{v};
    }

    template <std::floating_point T>
    [[nodiscard]] constexpr degrees<T> deg(T v) noexcept
    {
        return degrees<T>{v};
    }

    template <std::floating_point T>
    [[nodiscard]] constexpr radians<T> to_radians(degrees<T> d) noexcept
    {
        return radians<T>{d.count() * (std::numbers::pi_v<T> / T{180})};
    }

    template <std::floating_point T>
    [[nodiscard]] constexpr degrees<T> to_degrees(radians<T> r) noexcept
    {
        return degrees<T>{r.count() * (T{180} / std::numbers::pi_v<T>)};
    }

    using radiansf = radians<float>;
    using radiansd = radians<double>;
    using degreesf = degrees<float>;
    using degreesd = degrees<double>;

    namespace literals
    {
        [[nodiscard]] constexpr radians<long double> operator""_rad(long double v) noexcept
        {
            return radians<long double>{v};
        }

        [[nodiscard]] constexpr radians<long double> operator""_rad(unsigned long long v) noexcept
        {
            return radians<long double>{static_cast<long double>(v)};
        }

        [[nodiscard]] constexpr degrees<long double> operator""_deg(long double v) noexcept
        {
            return degrees<long double>{v};
        }

        [[nodiscard]] constexpr degrees<long double> operator""_deg(unsigned long long v) noexcept
        {
            return degrees<long double>{static_cast<long double>(v)};
        }
    } // namespace literals

} // namespace catalyst::math
