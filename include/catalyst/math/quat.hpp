/**
 * @file quat.hpp
 * @brief A simple quaternion type with basic operations for 3D rotations.
 * @details The quat template represents a quaternion, which is a mathematical construct used to represent rotations in 3D space. It provides basic operations such as normalization, conjugation, inversion, and quaternion multiplication (Hamilton product). Additionally, it includes a function to rotate a vector by the quaternion and a static function to create a quaternion from an axis-angle representation. The quat type is designed to be simple and efficient for common use cases in graphics programming and game development.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include <catalyst/math/euler.hpp>
#include <catalyst/math/mat.hpp>
#include <catalyst/math/vec.hpp>

#include <concepts>
#include <cmath>
#include <cstddef>
#include <type_traits>

/**
 * @namespace catalyst::math
 * @brief The catalyst::math namespace contains all the mathematical types and functions provided by the Catalyst Math library. This includes vector and matrix types, quaternion support, angle types with user-defined literals, projection matrix utilities, transformation functions, and more. By organizing all math-related functionality within this namespace, we can avoid naming conflicts and provide a clear structure for users of the library to access the various mathematical tools they need for graphics programming and game development.
 * @details The Catalyst Math library is designed to be efficient, easy to use, and compatible with modern C++ standards. It provides a comprehensive set of mathematical utilities that are commonly used in graphics applications, making it easier for developers to perform complex mathematical operations without having to implement them from scratch. By including the appropriate headers from the Catalyst Math library, users can access all the functionality they need for their projects.
 */
namespace catalyst::math
{

	/**
	 * @struct quat
	 * @tparam T Scalar type for the quaternion components (e.g. float, double).
	 * @brief A simple quaternion type with basic operations for 3D rotations. Provides normalization, conjugation, inversion, quaternion multiplication (Hamilton product), vector rotation, and construction from axis-angle representation.
	 * @details The quat template represents a quaternion, which is a mathematical construct used to represent rotations in 3D space. It provides basic operations such as normalization, conjugation, inversion, and quaternion multiplication (Hamilton product). Additionally, it includes a function to rotate a vector by the quaternion and a static function to create a quaternion from an axis-angle representation. The quat type is designed to be simple and efficient for common use cases in graphics programming and game development.
	 */
	template <std::floating_point T>
	struct quat
	{
		/**
		 * @brief The scalar type used for the quaternion components (e.g. float, double).
		 */
		using value_type = T;

		/**
		 * @brief The x component of the quaternion, representing the i component in the quaternion (x, y, z, w).
		 */
		T x{};
		/**
		 * @brief The y component of the quaternion, representing the j component in the quaternion (x, y, z, w).
		 */
		T y{};
		/**
		 * @brief The z component of the quaternion, representing the k component in the quaternion (x, y, z, w).
		 */
		T z{};
		/**
		 * @brief The w component of the quaternion, representing the scalar part in the quaternion (x, y, z, w).
		 */
		T w{1};

		/**
		 * @brief Default constructor initializes the quaternion to the identity rotation (0, 0, 0, 1).
		 */
		constexpr quat() noexcept = default;
		/**
		 * @brief Constructs a quaternion from the given components (x, y, z, w).
		 * @param x_ The x component of the quaternion, representing the i component in the quaternion (x, y, z, w).
		 * @param y_ The y component of the quaternion, representing the j component in the quaternion (x, y, z, w).
		 * @param z_ The z component of the quaternion, representing the k component in the quaternion (x, y, z, w).
		 * @param w_ The w component of the quaternion, representing the scalar part in the quaternion (x, y, z, w).
		 */
		constexpr quat(T x_, T y_, T z_, T w_) noexcept : x(x_), y(y_), z(z_), w(w_) {}

		/**
		 * @fn identity
		 * @brief Returns the identity quaternion representing no rotation (0, 0, 0, 1).
		 * @return A quaternion representing the identity rotation, where the vector part is zero and the scalar part is one. This quaternion can be used as a neutral element in quaternion multiplication, meaning that multiplying any quaternion by the identity quaternion will yield the original quaternion unchanged. The identity quaternion is commonly used as a starting point for constructing rotations or as a default value when no rotation is desired.
		 */
		[[nodiscard]] static constexpr quat identity() noexcept { return quat{T{}, T{}, T{}, T{1}}; }

		/**
		 * @fn xyz
		 * @brief Returns the vector part of the quaternion as a 3D vector (x, y, z).
		 * @return A 3D vector containing the x, y, and z components of the quaternion. This vector represents the imaginary part of the quaternion, which encodes the axis of rotation when the quaternion is used to represent a rotation in 3D space. The w component of the quaternion represents the scalar part, which encodes the angle of rotation. The xyz function provides a convenient way to access the vector part of the quaternion for operations such as rotating vectors or constructing quaternions from axis-angle representations.
		 */
		[[nodiscard]] constexpr vec<T, 3> xyz() const noexcept { return vec<T, 3>{x, y, z}; }

		/**
		 * @fn length_sq
		 * @brief Returns the squared length (magnitude) of the quaternion.
		 * @return The squared length of the quaternion, calculated as x^2 + y^2 + z^2 + w^2. This value is useful for operations that require the magnitude of the quaternion without the computational cost of taking a square root, such as normalization or checking if the quaternion is close to zero. If the squared length is zero, it indicates that the quaternion is a zero quaternion, which does not represent a valid rotation and should be handled accordingly (e.g., by returning the identity quaternion or avoiding normalization).
		 */
		[[nodiscard]] constexpr T length_sq() const noexcept
		{
			return x * x + y * y + z * z + w * w;
		}

		/**
		 * @fn length
		 * @brief Returns the length (magnitude) of the quaternion.
		 * @return The length of the quaternion, calculated as the square root of the sum of the squares of its components (i.e., sqrt(x^2 + y^2 + z^2 + w^2)). This value represents the magnitude of the quaternion and is used in operations such as normalization. If the length is zero, it indicates that the quaternion is a zero quaternion, which does not represent a valid rotation and should be handled accordingly (e.g., by returning the identity quaternion or avoiding normalization).
		 */
		[[nodiscard]] T length() const noexcept
		{
			return static_cast<T>(std::sqrt(static_cast<long double>(length_sq())));
		}

		/**
		 * @fn normalized
		 * @brief Returns a normalized (unit) quaternion with the same direction as the original.
		 * @return A new quaternion that is the normalized version of the original quaternion. The normalized quaternion has a length of 1 and represents the same rotation as the original quaternion. If the original quaternion has a length of zero (i.e., it is a zero quaternion), the function returns the identity quaternion to avoid division by zero. Normalization is commonly used to ensure that quaternions represent valid rotations, as unit quaternions are required for proper rotation operations in 3D space.
		 */
		[[nodiscard]] quat normalized() const noexcept
		{
			const T len = length();
			if (len == T{})
				return identity();
			const T inv = T{1} / len;
			return quat{x * inv, y * inv, z * inv, w * inv};
		}

		/**
		 * @fn conjugate
		 * @brief Returns the conjugate of the quaternion, which negates the vector part and keeps the scalar part unchanged.
		 * @return A new quaternion that is the conjugate of the original quaternion. The conjugate of a quaternion (x, y, z, w) is defined as (-x, -y, -z, w). This operation is useful for various quaternion calculations, such as computing the inverse of a quaternion or rotating vectors in the opposite direction. The conjugate of a unit quaternion represents the inverse rotation, while for non-unit quaternions, it is used in conjunction with the length to compute the inverse.
		 */
		[[nodiscard]] constexpr quat conjugate() const noexcept
		{
			return quat{-x, -y, -z, w};
		}

		/**
		 * @fn inverse
		 * @brief Returns the inverse of the quaternion, which represents the opposite rotation.
		 * @return A new quaternion that is the inverse of the original quaternion. The inverse of a quaternion q is calculated as the conjugate of q divided by the squared length of q (i.e., q^-1 = conjugate(q) / length_sq(q)). If the original quaternion has a length of zero (i.e., it is a zero quaternion), the function returns the identity quaternion to avoid division by zero. The inverse of a quaternion can be used to reverse a rotation or to compute relative rotations between quaternions.
		 */
		[[nodiscard]] quat inverse() const noexcept
		{
			const T lsq = length_sq();
			if (lsq == T{})
				return identity();
			const T inv = T{1} / lsq;
			const quat c = conjugate();
			return quat{c.x * inv, c.y * inv, c.z * inv, c.w * inv};
		}

		/**
		 * @fn operator*
		 * @brief Multiplies two quaternions using the Hamilton product and returns the result. This operation combines the rotations represented by the two quaternions.
		 * @param a The first quaternion operand.
		 * @param b The second quaternion operand to be multiplied with the first.
		 * @return A new quaternion that is the result of multiplying quaternion a by quaternion b using the Hamilton product. The resulting quaternion represents the combined rotation of a followed by b. The Hamilton product is defined as follows:
		 *   - x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y
		 *   - y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x
		 *   - z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w
		 *   - w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
		 */
		[[nodiscard]] friend constexpr quat operator*(const quat &a, const quat &b) noexcept
		{
			return quat{
				a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
				a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
				a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
				a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
			};
		}

		/**
		 * @fn operator*=
		 * @brief Multiplies this quaternion by another quaternion in-place using the Hamilton product. This operation combines the rotations represented by this quaternion and the other quaternion, updating this quaternion to represent the combined rotation.
		 * @param other The quaternion to multiply with this quaternion.
		 * @return Reference to this quat after the multiplication, allowing for chaining of operations. The components of this quaternion are updated to be the result of multiplying this quaternion by the other quaternion using the Hamilton product. The resulting quaternion represents the combined rotation of this followed by other.
		 */
		constexpr quat &operator*=(const quat &other) noexcept { return *this = (*this * other); }

		/**
		 * @fn operator*
		 * @brief Multiplies a quaternion by a scalar value and returns the result. This operation scales the rotation represented by the quaternion by the scalar value, which can be useful for operations such as interpolation or adjusting the angle of rotation.
		 * @param q The quaternion operand to be multiplied by the scalar.
		 * @param s The scalar value to multiply the quaternion by.
		 * @return A new quaternion that is the result of multiplying each component of quaternion q by the scalar value s. Each component of the resulting quaternion is calculated as q.x * s, q.y * s, q.z * s, and q.w * s. This operation effectively scales the rotation represented by the quaternion by the factor of s. If s is greater than 1, the rotation will be amplified; if s is between 0 and 1, the rotation will be reduced; if s is negative, the rotation will be reversed and scaled.
		 */
		[[nodiscard]] friend constexpr quat operator*(quat q, T s) noexcept { return quat{q.x * s, q.y * s, q.z * s, q.w * s}; }
		/**
		 * @fn operator*
		 * @brief Multiplies a scalar value by a quaternion and returns the result. This operation scales the rotation represented by the quaternion by the scalar value, which can be useful for operations such as interpolation or adjusting the angle of rotation. This operator allows for commutative scalar multiplication, meaning that the order of the operands does not affect the result (i.e., s * q is equivalent to q * s).
		 * @param s The scalar value to multiply the quaternion by.
		 * @param q The quaternion operand to be multiplied by the scalar.
		 * @return A new quaternion that is the result of multiplying each component of quaternion q by the scalar value s. Each component of the resulting quaternion is calculated as s * q.x, s * q.y, s * q.z, and s * q.w. This operation effectively scales the rotation represented by the quaternion by the factor of s. If s is greater than 1, the rotation will be amplified; if s is between 0 and 1, the rotation will be reduced; if s is negative, the rotation will be reversed and scaled. This operator provides flexibility in how scalar multiplication can be expressed in code, allowing for more natural mathematical expressions involving quaternions and scalars.
		 */
		[[nodiscard]] friend constexpr quat operator*(T s, quat q) noexcept { return q * s; }

		/**
		 * @fn operator+
		 * @brief Adds two quaternions element-wise and returns the result. This operation is not commonly used for combining rotations, but it can be useful for certain interpolation techniques or when working with quaternion derivatives.
		 * @param a The first quaternion operand.
		 * @param b The second quaternion operand.
		 * @return A new quaternion that is the result of adding each component of quaternion a to the corresponding component of quaternion b. Each component of the resulting quaternion is calculated as a.x + b.x, a.y + b.y, a.z + b.z, and a.w + b.w.
		 */
		[[nodiscard]] friend constexpr quat operator+(quat a, const quat &b) noexcept { return quat{a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
		/**
		 * @fn operator-
		 * @brief Subtracts two quaternions element-wise and returns the result. This operation is not commonly used for combining rotations, but it can be useful for certain interpolation techniques or when working with quaternion derivatives.
		 * @param a The first quaternion operand.
		 * @param b The second quaternion operand to be subtracted from the first.
		 * @return A new quaternion that is the result of subtracting each component of quaternion b from the corresponding component of quaternion a. Each component of the resulting quaternion is calculated as a.x - b.x, a.y - b.y, a.z - b.z, and a.w - b.w.
		 */
		[[nodiscard]] friend constexpr quat operator-(quat a, const quat &b) noexcept { return quat{a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w}; }

		/**
		 * @fn operator==
		 * @brief Compares two quaternions for equality by checking if all components are equal.
		 * @param a The first quaternion operand.
		 * @param b The second quaternion operand to compare with the first.
		 * @return true if all components of quaternion a are equal to the corresponding components of quaternion b (i.e., a.x == b.x, a.y == b.y, a.z == b.z, and a.w == b.w); otherwise, returns false. This operator checks for exact equality of the quaternion components, which may not be suitable for comparing quaternions that represent the same rotation but have different magnitudes or signs. In practice, it may be necessary to use an approximate comparison with a tolerance when comparing quaternions for equivalence in terms of rotation.
		 */
		[[nodiscard]] friend constexpr bool operator==(const quat &a, const quat &b) noexcept
		{
			return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
		}
		/**
		 * @fn operator!=
		 * @brief Compares two quaternions for inequality by checking if any component is not equal.
		 * @param a The first quaternion operand.
		 * @param b The second quaternion operand to compare with the first.
		 * @return true if any component of quaternion a is not equal to the corresponding component of quaternion b (i.e., a.x != b.x, a.y != b.y, a.z != b.z, or a.w != b.w); otherwise, returns false. This operator checks for exact inequality of the quaternion components, which may not be suitable for comparing quaternions that represent the same rotation but have different magnitudes or signs. In practice, it may be necessary to use an approximate comparison with a tolerance when comparing quaternions for equivalence in terms of rotation.
		 */
		[[nodiscard]] friend constexpr bool operator!=(const quat &a, const quat &b) noexcept { return !(a == b); }

		/**
		 * @fn rotate
		 * @brief Rotates a 3D vector by the rotation represented by this quaternion and returns the rotated vector. This operation applies the rotation encoded by the quaternion to the input vector, resulting in a new vector that is rotated in 3D space.
		 * @param v The 3D vector to be rotated by this quaternion.
		 * @return A new 3D vector that is the result of rotating the input vector v by the rotation represented by this quaternion. The rotation is applied using the formula: v' = v + 2*w*(q.xyz x v) + 2*(q.xyz x (q.xyz x v)), where q.xyz is the vector part of the quaternion and w is the scalar part. This formula efficiently applies the rotation without needing to convert the quaternion to a rotation matrix, making it suitable for real-time applications such as graphics rendering and game development.
		 */
		[[nodiscard]] constexpr vec<T, 3> rotate(const vec<T, 3> &v) const noexcept
		{
			// v' = v + 2*w*(q.xyz x v) + 2*(q.xyz x (q.xyz x v))
			const vec<T, 3> qv{x, y, z};
			const vec<T, 3> t = T{2} * qv.cross(v);
			return v + (w * t) + qv.cross(t);
		}

		/**
		 * @fn from_axis_angle
		 * @brief Creates a quaternion representing a rotation of a specified angle around a given axis. The axis is expected to be a non-zero vector, and the angle is specified in radians.
		 * @param axis The 3D vector representing the axis of rotation. This vector should not be the zero vector, as it will be normalized internally to determine the direction of the rotation. The components of the axis vector (x, y, z) represent the direction of the rotation in 3D space.
		 * @param radians The angle of rotation in radians. A positive angle represents a counterclockwise rotation around the axis when looking from the tip of the axis vector towards the origin. A negative angle represents a clockwise rotation.
		 * @tparam T Scalar type for the quaternion components and parameters (e.g., float, double).
		 * @return A quaternion that represents a rotation of the specified angle around the given axis. The resulting quaternion is calculated using the formula: q = (sin(angle/2) * normalized_axis, cos(angle/2)), where normalized_axis is the input axis vector normalized to unit length. If the input axis is a zero vector, the function returns the identity quaternion, which represents no rotation.
		 */
		[[nodiscard]] static quat from_axis_angle(const vec<T, 3> &axis, T radians) noexcept
		{
			const T half = radians * T{0.5};
			const T s = static_cast<T>(std::sin(static_cast<long double>(half)));
			const T c = static_cast<T>(std::cos(static_cast<long double>(half)));

			const T axis_len = axis.length();
			if (axis_len == T{})
				return identity();

			const vec<T, 3> n = axis / axis_len;
			return quat{n.x * s, n.y * s, n.z * s, c};
		}

		/**
		 * @fn from_axis_angle_degrees
		 * @brief Creates a quaternion representing a rotation of a specified angle around a given axis, where the angle is specified in degrees. The axis is expected to be a non-zero vector, and the function internally converts the angle from degrees to radians before creating the quaternion.
		 * @param axis The 3D vector representing the axis of rotation. This vector should not be the zero vector, as it will be normalized internally to determine the direction of the rotation. The components of the axis vector (x, y, z) represent the direction of the rotation in 3D space.
		 * @param degrees The angle of rotation in degrees. A positive angle represents a counterclockwise rotation around the axis when looking from the tip of the axis vector towards the origin. A negative angle represents a clockwise rotation.
		 * @tparam T Scalar type for the quaternion components and parameters (e.g., float, double).
		 * @return A quaternion that represents a rotation of the specified angle around the given axis. The resulting quaternion is calculated by first converting the input angle from degrees to radians using the formula: radians = degrees * (pi / 180), and then using the same formula as from_axis_angle to create the quaternion. If the input axis is a zero vector, the function returns the identity quaternion, which represents no rotation.
		 */
		[[nodiscard]] static quat from_axis_angle(const vec<T, 3> &axis, catalyst::math::radians<T> radians) noexcept
		{
			return from_axis_angle(axis, radians.count());
		}

		/**
		 * @fn from_axis_angle_degrees
		 * @brief Creates a quaternion representing a rotation of a specified angle around a given axis, where the angle is specified in degrees. The axis is expected to be a non-zero vector, and the function internally converts the angle from degrees to radians before creating the quaternion.
		 * @param axis The 3D vector representing the axis of rotation. This vector should not be the zero vector, as it will be normalized internally to determine the direction of the rotation. The components of the axis vector (x, y, z) represent the direction of the rotation in 3D space.
		 * @param degrees The angle of rotation in degrees. A positive angle represents a counterclockwise rotation around the axis when looking from the tip of the axis vector towards the origin. A negative angle represents a clockwise rotation.
		 * @tparam T Scalar type for the quaternion components and parameters (e.g., float, double).
		 * @return A quaternion that represents a rotation of the specified angle around the given axis. The resulting quaternion is calculated by first converting the input angle from degrees to radians using the formula: radians = degrees * (pi / 180), and then using the same formula as from_axis_angle to create the quaternion. If the input axis is a zero vector, the function returns the identity quaternion, which represents no rotation.
		 */
		[[nodiscard]] static quat from_axis_angle_degrees(const vec<T, 3> &axis, catalyst::math::degrees<T> degrees) noexcept
		{
			return from_axis_angle(axis, catalyst::math::to_radians(degrees).count());
		}

		/**
		 * @fn from_yaw_pitch_roll
		 * @brief Creates a quaternion from the given yaw, pitch, and roll angles. The angles are expected to be in radians and follow the standard yaw (Z), pitch (Y), roll (X) convention. This function converts the Euler angles to a quaternion representation, which can be used for efficient rotation operations in 3D space.
		 * @param yaw The yaw angle in radians, representing rotation around the Z-axis.
		 * @param pitch The pitch angle in radians, representing rotation around the Y-axis.
		 * @param roll The roll angle in radians, representing rotation around the X-axis.
		 * @return A quaternion that represents the combined rotation specified by the yaw, pitch, and roll angles. The resulting quaternion is calculated using the standard conversion from Euler angles to quaternions, which involves computing the half-angles and applying trigonometric functions to derive the quaternion components. The function assumes a specific order of rotations (yaw, then pitch, then roll) and uses the corresponding formulas to compute the quaternion. This allows for easy construction of quaternions from common Euler angle representations used in graphics and game development.
		 */
		[[nodiscard]] static quat from_yaw_pitch_roll(T yaw, T pitch, T roll) noexcept
		{
			const yaw_pitch_roll<T> a{yaw, pitch, roll};
			const auto h = detail::trig_half(a);

			// Common yaw(Z), pitch(Y), roll(X) convention.
			const T w_ = (h.cr * h.cp * h.cy) + (h.sr * h.sp * h.sy);
			const T x_ = (h.sr * h.cp * h.cy) - (h.cr * h.sp * h.sy);
			const T y_ = (h.cr * h.sp * h.cy) + (h.sr * h.cp * h.sy);
			const T z_ = (h.cr * h.cp * h.sy) - (h.sr * h.sp * h.cy);
			return quat{x_, y_, z_, w_};
		}

		/**
		 * @fn from_yaw_pitch_roll
		 * @brief Creates a quaternion from the given yaw, pitch, and roll angles. The angles are expected to be in radians and follow the standard yaw (Z), pitch (Y), roll (X) convention. This function converts the Euler angles to a quaternion representation, which can be used for efficient rotation operations in 3D space.
		 * @param yaw The yaw angle in radians, representing rotation around the Z-axis.
		 * @param pitch The pitch angle in radians, representing rotation around the Y-axis.
		 * @param roll The roll angle in radians, representing rotation around the X-axis.
		 * @return A quaternion that represents the combined rotation specified by the yaw, pitch, and roll angles. The resulting quaternion is calculated using the standard conversion from Euler angles to quaternions, which involves computing the half-angles and applying trigonometric functions to derive the quaternion components. The function assumes a specific order of rotations (yaw, then pitch, then roll) and uses the corresponding formulas to compute the quaternion. This allows for easy construction of quaternions from common Euler angle representations used in graphics and game development.
		 */
		[[nodiscard]] static quat from_yaw_pitch_roll(catalyst::math::radians<T> yaw, catalyst::math::radians<T> pitch, catalyst::math::radians<T> roll) noexcept
		{
			return from_yaw_pitch_roll(yaw.count(), pitch.count(), roll.count());
		}
		
		/**
		 * @fn from_yaw_pitch_roll_degrees
		 * @brief Creates a quaternion from the given yaw, pitch, and roll angles in degrees. This function converts the degrees to radians and then uses the standard yaw (Z), pitch (Y), roll (X) convention to create the quaternion. It is useful for applications where angles are provided in degrees rather than radians.
		 * @param yaw_deg The yaw angle in degrees, representing rotation around the Z-axis.
		 * @param pitch_deg The pitch angle in degrees, representing rotation around the Y-axis.
		 * @param roll_deg The roll angle in degrees, representing rotation around the X-axis.
		 * @return A quaternion that represents the combined rotation specified by the yaw, pitch, and roll angles in degrees. The function internally converts the degrees to radians and then computes the quaternion using the standard conversion from Euler angles to quaternions.
		 */
		[[nodiscard]] static quat from_yaw_pitch_roll_degrees(catalyst::math::degrees<T> yaw_deg, catalyst::math::degrees<T> pitch_deg, catalyst::math::degrees<T> roll_deg) noexcept
		{
			const auto a = yaw_pitch_roll_from_degrees(yaw_deg, pitch_deg, roll_deg);
			return from_yaw_pitch_roll(a);
		}

		/**
		 * @fn from_yaw_pitch_roll
		 * @brief Creates a quaternion from the given yaw, pitch, and roll angles encapsulated in a yaw_pitch_roll struct. The angles are expected to be in radians and follow the standard yaw (Z), pitch (Y), roll (X) convention. This function provides a convenient way to create a quaternion from a single struct that contains all three Euler angles.
		 * @param a A yaw_pitch_roll struct containing the yaw, pitch, and roll angles in radians.
		 * @return A quaternion that represents the combined rotation specified by the yaw, pitch, and roll angles contained in the input struct. The resulting quaternion is calculated using the standard conversion from Euler angles to quaternions, which involves computing the half-angles and applying trigonometric functions to derive the quaternion components. The function assumes a specific order of rotations (yaw, then pitch, then roll) and uses the corresponding formulas to compute the quaternion. This allows for easy construction of quaternions from common Euler angle representations used in graphics and game development.
		 */
		[[nodiscard]] static quat from_yaw_pitch_roll(const yaw_pitch_roll<T> &a) noexcept
		{
			return from_yaw_pitch_roll(a.yaw, a.pitch, a.roll);
		}

		/**
		 * @fn from_yaw_pitch_roll_degrees
		 * @brief Creates a quaternion from the given yaw, pitch, and roll angles encapsulated in a yaw_pitch_roll struct, where the angles are specified in degrees. This function converts the degrees to radians and then uses the standard yaw (Z), pitch (Y), roll (X) convention to create the quaternion. It is useful for applications where angles are provided in degrees rather than radians.
		 * @param a A yaw_pitch_roll struct containing the yaw, pitch, and roll angles in degrees.
		 * @return A quaternion that represents the combined rotation specified by the yaw, pitch, and roll angles in degrees contained in the input struct. The function internally converts the degrees to radians and then computes the quaternion using the standard conversion from Euler angles to quaternions.
		 */
		[[nodiscard]] static quat from_yaw_pitch_roll_degrees(T yaw_deg, T pitch_deg, T roll_deg) noexcept
		{
			const auto a = yaw_pitch_roll_from_degrees<T>(yaw_deg, pitch_deg, roll_deg);
			return from_yaw_pitch_roll(a);
		}

		/**
		 * @fn to_yaw_pitch_roll
		 * @brief Converts this quaternion to the corresponding yaw, pitch, and roll angles in radians, following the standard yaw (Z), pitch (Y), roll (X) convention. This function extracts the Euler angles from the quaternion representation, which can be useful for applications that require Euler angles for user input, animation, or other purposes.
		 * @return A yaw_pitch_roll struct containing the yaw, pitch, and roll angles in radians that correspond to the rotation represented by this quaternion. The function first normalizes the quaternion to ensure it represents a valid rotation, and then uses standard formulas to extract the Euler angles based on the quaternion components. The resulting angles are returned in a struct for convenient access.
		 */
		[[nodiscard]] yaw_pitch_roll<T> to_yaw_pitch_roll() const noexcept
		{
			const quat q = this->normalized();

			// Standard ZYX extraction.
			const T sinp = T{2} * (q.w * q.y - q.z * q.x);
			const T pitch = static_cast<T>(std::asin(static_cast<long double>(detail::clamp(sinp, T{-1}, T{1}))));

			const T yaw = static_cast<T>(
				std::atan2(
					static_cast<long double>(T{2} * (q.w * q.z + q.x * q.y)),
					static_cast<long double>(T{1} - T{2} * (q.y * q.y + q.z * q.z))));

			const T roll = static_cast<T>(
				std::atan2(
					static_cast<long double>(T{2} * (q.w * q.x + q.y * q.z)),
					static_cast<long double>(T{1} - T{2} * (q.x * q.x + q.y * q.y))));

			return yaw_pitch_roll<T>{yaw, pitch, roll};
		}

		/**
		 * @fn to_mat3
		 * @brief Converts this quaternion to a corresponding 3x3 rotation matrix. The resulting matrix represents the same rotation as the quaternion and can be used for operations that require matrix representations of rotations, such as transforming vectors or combining with other transformations in graphics applications.
		 * @return A 3x3 matrix that represents the same rotation as this quaternion. The function first normalizes the quaternion to ensure it represents a valid rotation, and then computes the elements of the rotation matrix using the standard formulas derived from the quaternion components. The resulting matrix is in row-major form and can be used for various transformation operations in 3D graphics and game development.
		 */
		[[nodiscard]] mat<T, 3, 3> to_mat3() const noexcept
		{
			const quat q = this->normalized();

			const T xx = q.x * q.x;
			const T yy = q.y * q.y;
			const T zz = q.z * q.z;
			const T xy = q.x * q.y;
			const T xz = q.x * q.z;
			const T yz = q.y * q.z;
			const T wx = q.w * q.x;
			const T wy = q.w * q.y;
			const T wz = q.w * q.z;

			// Standard right-handed rotation matrix (row-major form), stored via m(row,col).
			mat<T, 3, 3> m{};
			m(0, 0) = T{1} - T{2} * (yy + zz);
			m(0, 1) = T{2} * (xy - wz);
			m(0, 2) = T{2} * (xz + wy);

			m(1, 0) = T{2} * (xy + wz);
			m(1, 1) = T{1} - T{2} * (xx + zz);
			m(1, 2) = T{2} * (yz - wx);

			m(2, 0) = T{2} * (xz - wy);
			m(2, 1) = T{2} * (yz + wx);
			m(2, 2) = T{1} - T{2} * (xx + yy);
			return m;
		}

		/**
		 * @fn to_mat4
		 * @brief Converts this quaternion to a corresponding 4x4 rotation matrix. The resulting matrix represents the same rotation as the quaternion and can be used for operations that require homogeneous transformation matrices, such as combining with translation or scaling transformations in graphics applications.
		 * @return A 4x4 matrix that represents the same rotation as this quaternion. The function first normalizes the quaternion to ensure it represents a valid rotation, and then computes the upper-left 3x3 portion of the matrix using the same formulas as to_mat3. The remaining elements of the matrix are set to form an identity transformation (i.e., the last row is [0, 0, 0, 1] and the last column is [0, 0, 0, 1]). This allows the resulting matrix to be used directly in homogeneous coordinate transformations commonly used in graphics and game development.
		 */
		[[nodiscard]] mat<T, 4, 4> to_mat4() const noexcept
		{
			mat<T, 4, 4> m = mat<T, 4, 4>::identity();
			const mat<T, 3, 3> r = to_mat3();
			for (std::size_t row = 0; row < 3; ++row)
			{
				for (std::size_t col = 0; col < 3; ++col)
					m(row, col) = r(row, col);
			}
			return m;
		}
	};

	/**
	 * @typedef quatf
	 * @brief A quaternion with float components, representing rotations in 3D space with single-precision floating-point values. This type is commonly used in graphics applications where performance is a concern and the precision of float is sufficient for representing rotations without noticeable artifacts.
	 */
	using quatf = quat<float>;
	/**
	 * @typedef quatd
	 * @brief A quaternion with double components, representing rotations in 3D space with double-precision floating-point values. This type is used in applications that require higher precision for representing rotations, such as scientific simulations or when working with very small angles where the increased precision of double can help reduce numerical errors.
	 */
	using quatd = quat<double>;

} // namespace catalyst::math

