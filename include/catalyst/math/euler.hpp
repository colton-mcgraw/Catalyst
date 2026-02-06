/**
 * @file euler.hpp
 * @brief Euler angle types and conversions for yaw-pitch-roll (intrinsic ZYX).
 * @details This header defines a simple struct for representing Euler angles in yaw-pitch-roll convention, as well as utility functions for converting between degrees and radians, and for constructing yaw_pitch_roll structs from degrees or radians. The angles are stored as raw scalar values in radians, and the struct provides a convenient way to represent and manipulate Euler angles in a consistent way.
 * License: MIT (see LICENSE).
 */
#pragma once

#include <catalyst/math/angle.hpp>

#include <cmath>
#include <concepts>
#include <numbers>
#include <type_traits>

/**
 * @namespace catalyst::math
 * @brief The catalyst::math namespace contains all the mathematical types and functions provided by the Catalyst Math library. This includes vector and matrix types, quaternion support, angle types with user-defined literals, projection matrix utilities, transformation functions, and more. By organizing all math-related functionality within this namespace, we can avoid naming conflicts and provide a clear structure for users of the library to access the various mathematical tools they need for graphics programming and game development.
 * @details The Catalyst Math library is designed to be efficient, easy to use, and compatible with modern C++ standards. It provides a comprehensive set of mathematical utilities that are commonly used in graphics applications, making it easier for developers to perform complex mathematical operations without having to implement them from scratch. By including the appropriate headers from the Catalyst Math library, users can access all the functionality they need for their projects.
 */
namespace catalyst::math
{

    /**
     * @fn pi_v
     * @brief A compile-time constant representing the value of pi for a given floating-point type.
     * @tparam T A floating-point type (e.g. float, double).
     * @return The value of pi as a constant of type T.
     */
    template <std::floating_point T>
    inline constexpr T pi_v = std::numbers::pi_v<T>;

    /**
     * @fn deg_to_rad
     * @brief Converts an angle from degrees to radians.
     * @tparam T A floating-point type (e.g. float, double).
     * @param degrees The angle in degrees to convert.
     * @return The equivalent angle in radians.
     */
    template <std::floating_point T>
    [[nodiscard]] constexpr T deg_to_rad(T degrees) noexcept
    {
        return degrees * (std::numbers::pi_v<T> / static_cast<T>(180));
    }

    /**
     * @fn deg_to_rad
     * @brief Converts a degrees angle to radians.
     * @tparam T Scalar type for the angle value (e.g. float, double).
     * @param degrees The degrees angle to convert.
     * @return A radians angle representing the same angle as degrees, but in radians.
     */
    template <std::floating_point T>
    [[nodiscard]] constexpr radians<T> deg_to_rad(degrees<T> degrees) noexcept
    {
        return to_radians(degrees);
    }

    /**
     * @fn rad_to_deg
     * @brief Converts an angle from radians to degrees.
     * @tparam T A floating-point type (e.g. float, double).
     * @param radians The angle in radians to convert.
     * @return The equivalent angle in degrees.
     */
    template <std::floating_point T>
    [[nodiscard]] constexpr T rad_to_deg(T radians) noexcept
    {
        return radians * (static_cast<T>(180) / std::numbers::pi_v<T>);
    }

    /**
     * @fn rad_to_deg
     * @brief Converts a radians angle to degrees.
     * @tparam T Scalar type for the angle value (e.g. float, double).
     * @param radians The radians angle to convert.
     * @return A degrees angle representing the same angle as radians, but in degrees.
     */
    template <std::floating_point T>
    [[nodiscard]] constexpr degrees<T> rad_to_deg(radians<T> radians) noexcept
    {
        return to_degrees(radians);
    }

    /**
     * @struct yaw_pitch_roll
     * @tparam T Scalar type for the angle values (e.g. float, double).
     * @brief A simple struct representing Euler angles in yaw-pitch-roll (intrinsic ZYX) convention. The angles are stored as raw scalar values in radians, and the struct provides utility functions for constructing from degrees or radians.
     */
    template <std::floating_point T>
    struct yaw_pitch_roll
    {
        /**
         * @brief The yaw angle (rotation around the Z-axis) in radians.
         */
        T yaw{};
        /**
         * @brief The pitch angle (rotation around the Y-axis) in radians.
         */
        T pitch{};
        /**
         * @brief The roll angle (rotation around the X-axis) in radians.
         */
        T roll{};
    };

    /**
     * @fn yaw_pitch_roll_from_degrees
     * @brief Constructs a yaw_pitch_roll struct from angles specified in degrees.
     * @param yaw_deg The yaw angle in degrees.
     * @param pitch_deg The pitch angle in degrees.
     * @param roll_deg The roll angle in degrees.
     * @tparam T Scalar type for the angle values (e.g. float, double).
     * @return A yaw_pitch_roll struct with the angles converted to radians.
     */
    template <std::floating_point T>
    [[nodiscard]] constexpr yaw_pitch_roll<T> yaw_pitch_roll_from_degrees(T yaw_deg, T pitch_deg, T roll_deg) noexcept
    {
        return yaw_pitch_roll<T>{deg_to_rad(yaw_deg), deg_to_rad(pitch_deg), deg_to_rad(roll_deg)};
    }

    /**
     * @fn yaw_pitch_roll_from_radians
     * @brief Constructs a yaw_pitch_roll struct from angles specified in radians.
     * @param yaw The yaw angle in radians.
     * @param pitch The pitch angle in radians.
     * @param roll The roll angle in radians.
     * @tparam T Scalar type for the angle values (e.g. float, double).
     * @return A yaw_pitch_roll struct with the angles set to the given radians values.
     */
    template <std::floating_point T>
    [[nodiscard]] constexpr yaw_pitch_roll<T> yaw_pitch_roll_from_degrees(degrees<T> yaw_deg, degrees<T> pitch_deg, degrees<T> roll_deg) noexcept
    {
        return yaw_pitch_roll<T>{to_radians(yaw_deg).count(), to_radians(pitch_deg).count(), to_radians(roll_deg).count()};
    }

    /**
     * @fn yaw_pitch_roll_from_radians
     * @brief Constructs a yaw_pitch_roll struct from angles specified in radians.
     * @param yaw The yaw angle in radians.
     * @param pitch The pitch angle in radians.
     * @param roll The roll angle in radians.
     * @tparam T Scalar type for the angle values (e.g. float, double).
     * @return A yaw_pitch_roll struct with the angles set to the given radians values.
     */
    template <std::floating_point T>
    [[nodiscard]] constexpr yaw_pitch_roll<T> yaw_pitch_roll_from_radians(radians<T> yaw, radians<T> pitch, radians<T> roll) noexcept
    {
        return yaw_pitch_roll<T>{yaw.count(), pitch.count(), roll.count()};
    }

    namespace detail
    {
        /**
         * @fn clamp
         * @brief Clamps a value between a lower and upper bound.
         * @param v The value to clamp.
         * @param lo The lower bound of the clamp range.
         * @param hi The upper bound of the clamp range.
         * @tparam T A type that supports comparison and is the same for v, lo, and hi.
         * @return The clamped value, which will be equal to v if it is within the range [lo, hi], or equal to lo if v is less than lo, or equal to hi if v is greater than hi.
         */
        template <typename T>
        [[nodiscard]] constexpr T clamp(T v, T lo, T hi) noexcept
        {
            return (v < lo) ? lo : ((v > hi) ? hi : v);
        }

        /**
         * @struct ypr_trig
         * @tparam T Scalar type for the trigonometric values (e.g. float, double).
         * @brief A helper struct to store the sine and cosine of yaw, pitch, and roll angles for efficient computation of rotation matrices. This struct is used internally to avoid redundant calculations of sine and cosine values when constructing rotation matrices from yaw-pitch-roll angles.
         */
        template <typename T>
        struct ypr_trig
        {
            /**
             * @brief The cosine of the yaw angle.
             */
            T cy{};
            /**
             * @brief The sine of the yaw angle.
             */
            T sy{};
            /**
             * @brief The cosine of the pitch angle.
             */
            T cp{};
            /**
             * @brief The sine of the pitch angle.
             */
            T sp{};
            /**
             * @brief The cosine of the roll angle.
             */
            T cr{};
            /**
             * @brief The sine of the roll angle.
             */
            T sr{};
        };

        /**
         * @struct yaw_pitch_roll_half_trig
         * @tparam T Scalar type for the trigonometric values (e.g. float, double).
         * @brief A helper struct to store the sine and cosine of half the yaw, pitch, and roll angles for efficient computation of quaternions from yaw-pitch-roll angles. This struct is used internally to avoid redundant calculations of sine and cosine values when constructing quaternions from yaw-pitch-roll angles.
         */
        template <typename T>
        struct ypr_half_trig
        {
            /**
             * @brief The cosine of half the yaw angle.
             */
            T cy{};
            /**
             * @brief The sine of half the yaw angle.
             */
            T sy{};
            /**
             * @brief The cosine of half the pitch angle.
             */
            T cp{};
            /**
             * @brief The sine of half the pitch angle.
             */
            T sp{};
            /**
             * @brief The cosine of half the roll angle.
             */
            T cr{};
            /**
             * @brief The sine of half the roll angle.
             */
            T sr{};
        };

        /**
         * @fn trig_full
         * @brief Computes the sine and cosine of the yaw, pitch, and roll angles for a given yaw_pitch_roll struct.
         * @param a The yaw_pitch_roll struct containing the yaw, pitch, and roll angles in radians.
         * @tparam T Scalar type for the angle values (e.g. float, double).
         * @return A ypr_trig struct containing the sine and cosine of the yaw, pitch, and roll angles.
         */
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

        /**
         * @fn trig_half
         * @brief Computes the sine and cosine of half the yaw, pitch, and roll angles for a given yaw_pitch_roll struct.
         * @param a The yaw_pitch_roll struct containing the yaw, pitch, and roll angles in radians.
         * @tparam T Scalar type for the angle values (e.g. float, double).
         * @return A ypr_half_trig struct containing the sine and cosine of half the yaw, pitch, and roll angles.
         */
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
