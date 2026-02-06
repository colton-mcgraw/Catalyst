#pragma once

#include <catalyst/math/angle.hpp>

#include <cmath>
#include <concepts>
#include <numbers>
#include <type_traits>

namespace catalyst::math
{

    template <std::floating_point T>
    inline constexpr T pi_v = std::numbers::pi_v<T>;

    template <std::floating_point T>
    [[nodiscard]] constexpr T deg_to_rad(T degrees) noexcept
    {
        return degrees * (std::numbers::pi_v<T> / static_cast<T>(180));
    }

    template <std::floating_point T>
    [[nodiscard]] constexpr radians<T> deg_to_rad(degrees<T> degrees) noexcept
    {
        return to_radians(degrees);
    }

    template <std::floating_point T>
    [[nodiscard]] constexpr T rad_to_deg(T radians) noexcept
    {
        return radians * (static_cast<T>(180) / std::numbers::pi_v<T>);
    }

    template <std::floating_point T>
    [[nodiscard]] constexpr degrees<T> rad_to_deg(radians<T> radians) noexcept
    {
        return to_degrees(radians);
    }

    // Right-handed Euler angles (intrinsic ZYX):
    // - yaw   around +Z
    // - pitch around +Y
    // - roll  around +X
    // Composition matches: R = Rz(yaw) * Ry(pitch) * Rx(roll)
    template <std::floating_point T>
    struct yaw_pitch_roll
    {

        T yaw{};
        T pitch{};
        T roll{};
    };

    template <std::floating_point T>
    [[nodiscard]] constexpr yaw_pitch_roll<T> yaw_pitch_roll_from_degrees(T yaw_deg, T pitch_deg, T roll_deg) noexcept
    {
        return yaw_pitch_roll<T>{deg_to_rad(yaw_deg), deg_to_rad(pitch_deg), deg_to_rad(roll_deg)};
    }

    template <std::floating_point T>
    [[nodiscard]] constexpr yaw_pitch_roll<T> yaw_pitch_roll_from_degrees(degrees<T> yaw_deg, degrees<T> pitch_deg, degrees<T> roll_deg) noexcept
    {
        return yaw_pitch_roll<T>{to_radians(yaw_deg).count(), to_radians(pitch_deg).count(), to_radians(roll_deg).count()};
    }

    template <std::floating_point T>
    [[nodiscard]] constexpr yaw_pitch_roll<T> yaw_pitch_roll_from_radians(radians<T> yaw, radians<T> pitch, radians<T> roll) noexcept
    {
        return yaw_pitch_roll<T>{yaw.count(), pitch.count(), roll.count()};
    }

    namespace detail
    {
        template <typename T>
        [[nodiscard]] constexpr T clamp(T v, T lo, T hi) noexcept
        {
            return (v < lo) ? lo : ((v > hi) ? hi : v);
        }

        template <typename T>
        struct ypr_trig
        {
            T cy{}, sy{}; // cos/sin(yaw)
            T cp{}, sp{}; // cos/sin(pitch)
            T cr{}, sr{}; // cos/sin(roll)
        };

        template <typename T>
        struct ypr_half_trig
        {
            T cy{}, sy{}; // cos/sin(yaw/2)
            T cp{}, sp{}; // cos/sin(pitch/2)
            T cr{}, sr{}; // cos/sin(roll/2)
        };

        template <typename T>
        [[nodiscard]] inline ypr_trig<T> trig_full(const yaw_pitch_roll<T>& a) noexcept
        {
            using ld = long double;
            return ypr_trig<T>{
                static_cast<T>(std::cos(static_cast<ld>(a.yaw))),
                static_cast<T>(std::sin(static_cast<ld>(a.yaw))),
                static_cast<T>(std::cos(static_cast<ld>(a.pitch))),
                static_cast<T>(std::sin(static_cast<ld>(a.pitch))),
                static_cast<T>(std::cos(static_cast<ld>(a.roll))),
                static_cast<T>(std::sin(static_cast<ld>(a.roll))),
            };
        }

        template <typename T>
        [[nodiscard]] inline ypr_half_trig<T> trig_half(const yaw_pitch_roll<T>& a) noexcept
        {
            using ld = long double;
            const ld hy = static_cast<ld>(a.yaw) * 0.5L;
            const ld hp = static_cast<ld>(a.pitch) * 0.5L;
            const ld hr = static_cast<ld>(a.roll) * 0.5L;

            return ypr_half_trig<T>{
                static_cast<T>(std::cos(hy)), static_cast<T>(std::sin(hy)),
                static_cast<T>(std::cos(hp)), static_cast<T>(std::sin(hp)),
                static_cast<T>(std::cos(hr)), static_cast<T>(std::sin(hr)),
            };
        }
    }

} // namespace catalyst::math
