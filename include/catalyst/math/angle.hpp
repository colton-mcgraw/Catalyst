/**
 * @file angle.hpp
 * @brief Strongly-typed angle types for radians and degrees, with user-defined literals and conversion functions.
 * @details This header defines two strongly-typed angle types, radians and degrees, which wrap a raw scalar value representing the angle in radians or degrees, respectively. The types provide basic arithmetic operations, comparisons, and user-defined literals for convenient construction from floating-point or integer literals. Additionally, conversion functions are provided to convert between radians and degrees. The use of strongly-typed angle types helps prevent accidental mixing of raw scalar values with angles and improves code clarity.
 * License: MIT (see LICENSE).
 */

#pragma once

#include <compare>
#include <concepts>
#include <numbers>

/**
 * @namespace catalyst::math
 * @brief The catalyst::math namespace contains all the mathematical types and functions provided by the Catalyst Math library. This includes vector and matrix types, quaternion support, angle types with user-defined literals, projection matrix utilities, transformation functions, and more. By organizing all math-related functionality within this namespace, we can avoid naming conflicts and provide a clear structure for users of the library to access the various mathematical tools they need for graphics programming and game development.
 * @details The Catalyst Math library is designed to be efficient, easy to use, and compatible with modern C++ standards. It provides a comprehensive set of mathematical utilities that are commonly used in graphics applications, making it easier for developers to perform complex mathematical operations without having to implement them from scratch. By including the appropriate headers from the Catalyst Math library, users can access all the functionality they need for their projects.
 */
namespace catalyst::math
{

    /**
     * @struct radians
     * @tparam T Scalar type for the angle value (e.g. float, double).
     * @brief A strongly-typed angle type representing radians. Provides basic arithmetic operations, comparisons, and conversion to/from degrees. The underlying value is stored in radians, and the type ensures that angles are not accidentally mixed with raw scalar values.
     */
    template <std::floating_point T>
    struct radians
    {
        /**
         * @brief The scalar type used for the angle value (e.g. float, double).
         */
        using value_type = T;

        /**
         * @brief The angle value in radians.
         * @details The value is stored in radians, and all operations are performed in radians. To convert to degrees, use the provided conversion functions or user-defined literals.
         */
        T value{};

        /**
         * @brief Default constructor initializes the angle to zero radians.
         */
        constexpr radians() noexcept = default;
        /**
         * @brief Constructs a radians angle from a raw scalar value in radians.
         * @param v The angle value in radians.
         */
        constexpr explicit radians(T v) noexcept : value(v) {}

        /**
         * @brief Returns the raw angle value in radians.
         * @return The angle value in radians.
         */
        [[nodiscard]] constexpr T count() const noexcept { return value; }

        /**
         * @brief Default three-way comparison operator for radians. Compares the angle values in radians.
         * @param other The other radians angle to compare with.
         * @return A std::strong_ordering indicating the relative order of the angles.
         */
        constexpr auto operator<=>(const radians &) const = default;

        /**
         * @fn operator+
         * @brief Adds two radians angles together.
         * @param a The first radians angle.
         * @param b The second radians angle.
         * @return A new radians angle that is the sum of a and b.
         */
        [[nodiscard]] friend constexpr radians operator+(radians a, radians b) noexcept { return radians{a.value + b.value}; }
        /**
         * @fn operator-
         * @brief Subtracts one radians angle from another.
         * @param a The radians angle to subtract from.
         * @param b The radians angle to subtract.
         * @return A new radians angle that is the difference of a and b.
         */
        [[nodiscard]] friend constexpr radians operator-(radians a, radians b) noexcept { return radians{a.value - b.value}; }

        /**
         * @fn operator+=
         * @brief Adds another radians angle to this angle in-place.
         * @param other The radians angle to add to this angle.
         * @return Reference to this radians angle after the addition.
         */
        constexpr radians &operator+=(radians other) noexcept
        {
            value += other.value;
            return *this;
        }
        /**
         * @fn operator-=
         * @brief Subtracts another radians angle from this angle in-place.
         * @param other The radians angle to subtract from this angle.
         * @return Reference to this radians angle after the subtraction.
         */
        constexpr radians &operator-=(radians other) noexcept
        {
            value -= other.value;
            return *this;
        }

        /**
         * @fn operator*
         * @brief Multiplies a radians angle by a scalar value.
         * @param a The radians angle to multiply.
         * @param s The scalar value to multiply by.
         * @return A new radians angle that is the product of a and s.
         */
        [[nodiscard]] friend constexpr radians operator*(radians a, T s) noexcept { return radians{a.value * s}; }
        /**
         * @fn operator*
         * @brief Multiplies a scalar value by a radians angle.
         * @param s The scalar value to multiply.
         * @param a The radians angle to multiply.
         * @return A new radians angle that is the product of s and a.
         */
        [[nodiscard]] friend constexpr radians operator*(T s, radians a) noexcept { return radians{a.value * s}; }
        /**
         * @fn operator/
         * @brief Divides a radians angle by a scalar value.
         * @param a The radians angle to divide.
         * @param s The scalar value to divide by.
         * @return A new radians angle that is the quotient of a and s.
         */
        [[nodiscard]] friend constexpr radians operator/(radians a, T s) noexcept { return radians{a.value / s}; }

        /**
         * @fn operator*=
         * @brief Multiplies this radians angle by a scalar value in-place.
         * @param s The scalar value to multiply by.
         * @return Reference to this radians angle after the multiplication.
         */
        constexpr radians &operator*=(T s) noexcept
        {
            value *= s;
            return *this;
        }
        /**
         * @fn operator/=
         * @brief Divides this radians angle by a scalar value in-place.
         * @param s The scalar value to divide by.
         * @return Reference to this radians angle after the division.
         */
        constexpr radians &operator/=(T s) noexcept
        {
            value /= s;
            return *this;
        }
    };

    /**
     * @struct degrees
     * @tparam T Scalar type for the angle value (e.g. float, double).
     * @brief A strongly-typed angle type representing degrees. Provides basic arithmetic operations, comparisons, and conversion to/from radians. The underlying value is stored in degrees, and the type ensures that angles are not accidentally mixed with raw scalar values.
     */
    template <std::floating_point T>
    struct degrees
    {
        /**
         * @brief The scalar type used for the angle value (e.g. float, double).
         */
        using value_type = T;

        /**
         * @brief The angle value in degrees.
         * @details The value is stored in degrees, and all operations are performed in degrees. To convert to radians, use the provided conversion functions or user-defined literals.
         */
        T value{};

        /**
         * @brief Default constructor initializes the angle to zero degrees.
         */
        constexpr degrees() noexcept = default;
        /**
         * @brief Constructs a degrees angle from a raw scalar value in degrees.
         * @param v The angle value in degrees.
         */
        constexpr explicit degrees(T v) noexcept : value(v) {}

        /**
         * @brief Returns the raw angle value in degrees.
         * @return The angle value in degrees.
         */
        [[nodiscard]] constexpr T count() const noexcept { return value; }

        /**
         * @brief Default three-way comparison operator for degrees. Compares the angle values in degrees.
         * @param other The other degrees angle to compare with.
         * @return A std::strong_ordering indicating the relative order of the angles.
         */
        constexpr auto operator<=>(const degrees &) const = default;

        /**
         * @fn operator+
         * @brief Adds two degrees angles together.
         * @param a The first degrees angle.
         * @param b The second degrees angle.
         * @return A new degrees angle that is the sum of a and b.
         */
        [[nodiscard]] friend constexpr degrees operator+(degrees a, degrees b) noexcept { return degrees{a.value + b.value}; }
        /**
         * @fn operator-
         * @brief Subtracts one degrees angle from another.
         * @param a The degrees angle to subtract from.
         * @param b The degrees angle to subtract.
         * @return A new degrees angle that is the difference of a and b.
         */
        [[nodiscard]] friend constexpr degrees operator-(degrees a, degrees b) noexcept { return degrees{a.value - b.value}; }

        /**
         * @fn operator+=
         * @brief Adds another degrees angle to this one in-place.
         * @param other The degrees angle to add.
         * @return Reference to this degrees angle after the addition.
         */
        constexpr degrees &operator+=(degrees other) noexcept
        {
            value += other.value;
            return *this;
        }
        /**
         * @fn operator-=
         * @brief Subtracts another degrees angle from this one in-place.
         * @param other The degrees angle to subtract.
         * @return Reference to this degrees angle after the subtraction.
         */
        constexpr degrees &operator-=(degrees other) noexcept
        {
            value -= other.value;
            return *this;
        }

        /**
         * @fn operator*
         * @brief Multiplies a degrees angle by a scalar value.
         * @param a The degrees angle to multiply.
         * @param s The scalar value to multiply by.
         * @return A new degrees angle that is the product of a and s.
         */
        [[nodiscard]] friend constexpr degrees operator*(degrees a, T s) noexcept { return degrees{a.value * s}; }
        /**
         * @fn operator*
         * @brief Multiplies a scalar value by a degrees angle.
         * @param s The scalar value to multiply.
         * @param a The degrees angle to multiply.
         * @return A new degrees angle that is the product of s and a.
         */
        [[nodiscard]] friend constexpr degrees operator*(T s, degrees a) noexcept { return degrees{a.value * s}; }
        /**
         * @fn operator/
         * @brief Divides a degrees angle by a scalar value.
         * @param a The degrees angle to divide.
         * @param s The scalar value to divide by.
         * @return A new degrees angle that is the quotient of a and s.
         */
        [[nodiscard]] friend constexpr degrees operator/(degrees a, T s) noexcept { return degrees{a.value / s}; }

        /**
         * @fn operator*=
         * @brief Multiplies this degrees angle by a scalar value in-place.
         * @param s The scalar value to multiply by.
         * @return Reference to this degrees angle after the multiplication.
         */
        constexpr degrees &operator*=(T s) noexcept
        {
            value *= s;
            return *this;
        }
        /**
         * @fn operator/=
         * @brief Divides this degrees angle by a scalar value in-place.
         * @param s The scalar value to divide by.
         * @return Reference to this degrees angle after the division.
         */
        constexpr degrees &operator/=(T s) noexcept
        {
            value /= s;
            return *this;
        }
    };

    /**
     * @fn rad
     * @brief Constructs a radians angle from a raw scalar value in radians.
     * @param v The angle value in radians.
     * @return A radians angle representing the given value in radians.
     * @tparam T Scalar type for the angle value (e.g. float, double).
     */
    template <std::floating_point T>
    [[nodiscard]] constexpr radians<T> rad(T v) noexcept
    {
        return radians<T>{v};
    }

    /**
     * @fn deg
     * @brief Constructs a degrees angle from a raw scalar value in degrees.
     * @param v The angle value in degrees.
     * @return A degrees angle representing the given value in degrees.
     * @tparam T Scalar type for the angle value (e.g. float, double).
     */
    template <std::floating_point T>
    [[nodiscard]] constexpr degrees<T> deg(T v) noexcept
    {
        return degrees<T>{v};
    }

    /**
     * @fn to_radians
     * @brief Converts a degrees angle to radians.
     * @param d The degrees angle to convert.
     * @return A radians angle representing the same angle as d, but in radians.
     * @tparam T Scalar type for the angle value (e.g. float, double).
     */
    template <std::floating_point T>
    [[nodiscard]] constexpr radians<T> to_radians(degrees<T> d) noexcept
    {
        return radians<T>{d.count() * (std::numbers::pi_v<T> / T{180})};
    }

    /**
     * @fn to_degrees
     * @brief Converts a radians angle to degrees.
     * @param r The radians angle to convert.
     * @return A degrees angle representing the same angle as r, but in degrees.
     * @tparam T Scalar type for the angle value (e.g. float, double).
     */
    template <std::floating_point T>
    [[nodiscard]] constexpr degrees<T> to_degrees(radians<T> r) noexcept
    {
        return degrees<T>{r.count() * (T{180} / std::numbers::pi_v<T>)};
    }

    /**
     * @typedef radiansf
     * @brief A radians angle type with float precision.
     */
    using radiansf = radians<float>;
    /**
     * @typedef radiansd
     * @brief A radians angle type with double precision.
     */
    using radiansd = radians<double>;
    /**
     * @typedef degreesf
     * @brief A degrees angle type with float precision.
     */
    using degreesf = degrees<float>;
    /**
     * @typedef degreesd
     * @brief A degrees angle type with double precision.
     */
    using degreesd = degrees<double>;

    namespace literals
    {
        /**
         * @def operator""_rad
         * @brief User-defined literal for constructing radians angles from floating-point or integer literals. The literal value is interpreted as radians.
         * @param v The literal value in radians.
         * @return A radians angle representing the given value in radians.
         */
        [[nodiscard]] constexpr radians<long double> operator""_rad(long double v) noexcept
        {
            return radians<long double>{v};
        }

        /**
         * @def operator""_deg
         * @brief User-defined literal for constructing degrees angles from floating-point or integer literals. The literal value is interpreted as degrees.
         * @param v The literal value in degrees.
         * @return A degrees angle representing the given value in degrees.
         */
        [[nodiscard]] constexpr radians<long double> operator""_rad(unsigned long long v) noexcept
        {
            return radians<long double>{static_cast<long double>(v)};
        }

        /**
         * @def operator""_deg
         * @brief User-defined literal for constructing degrees angles from floating-point or integer literals. The literal value is interpreted as degrees.
         * @param v The literal value in degrees.
         * @return A degrees angle representing the given value in degrees.
         */
        [[nodiscard]] constexpr degrees<long double> operator""_deg(long double v) noexcept
        {
            return degrees<long double>{v};
        }

        /**
         * @def operator""_deg
         * @brief User-defined literal for constructing degrees angles from floating-point or integer literals. The literal value is interpreted as degrees.
         * @param v The literal value in degrees.
         * @return A degrees angle representing the given value in degrees.
         */
        [[nodiscard]] constexpr degrees<long double> operator""_deg(unsigned long long v) noexcept
        {
            return degrees<long double>{static_cast<long double>(v)};
        }
    } // namespace literals

} // namespace catalyst::math
