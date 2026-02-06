
#pragma once

#include <catalyst/math/euler.hpp>
#include <catalyst/math/mat.hpp>
#include <catalyst/math/vec.hpp>

#include <concepts>
#include <cmath>
#include <cstddef>
#include <type_traits>

namespace catalyst::math
{

	template <std::floating_point T>
	struct quat
	{
		using value_type = T;

		// Vector part (x,y,z) + scalar part (w)
		T x{};
		T y{};
		T z{};
		T w{1};

		constexpr quat() noexcept = default;
		constexpr quat(T x_, T y_, T z_, T w_) noexcept : x(x_), y(y_), z(z_), w(w_) {}

		[[nodiscard]] static constexpr quat identity() noexcept { return quat{T{}, T{}, T{}, T{1}}; }

		[[nodiscard]] constexpr vec<T, 3> xyz() const noexcept { return vec<T, 3>{x, y, z}; }

		[[nodiscard]] constexpr T length_sq() const noexcept
		{
			return x * x + y * y + z * z + w * w;
		}

		[[nodiscard]] T length() const noexcept
		{
			return static_cast<T>(std::sqrt(static_cast<long double>(length_sq())));
		}

		[[nodiscard]] quat normalized() const noexcept
		{
			const T len = length();
			if (len == T{})
				return identity();
			const T inv = T{1} / len;
			return quat{x * inv, y * inv, z * inv, w * inv};
		}

		[[nodiscard]] constexpr quat conjugate() const noexcept
		{
			return quat{-x, -y, -z, w};
		}

		[[nodiscard]] quat inverse() const noexcept
		{
			const T lsq = length_sq();
			if (lsq == T{})
				return identity();
			const T inv = T{1} / lsq;
			const quat c = conjugate();
			return quat{c.x * inv, c.y * inv, c.z * inv, c.w * inv};
		}

		// Hamilton product.
		[[nodiscard]] friend constexpr quat operator*(const quat &a, const quat &b) noexcept
		{
			return quat{
				a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
				a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
				a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
				a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
			};
		}

		constexpr quat &operator*=(const quat &other) noexcept { return *this = (*this * other); }

		[[nodiscard]] friend constexpr quat operator*(quat q, T s) noexcept { return quat{q.x * s, q.y * s, q.z * s, q.w * s}; }
		[[nodiscard]] friend constexpr quat operator*(T s, quat q) noexcept { return q * s; }

		[[nodiscard]] friend constexpr quat operator+(quat a, const quat &b) noexcept { return quat{a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
		[[nodiscard]] friend constexpr quat operator-(quat a, const quat &b) noexcept { return quat{a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w}; }

		[[nodiscard]] friend constexpr bool operator==(const quat &a, const quat &b) noexcept
		{
			return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
		}
		[[nodiscard]] friend constexpr bool operator!=(const quat &a, const quat &b) noexcept { return !(a == b); }

		// Rotate a vector by this quaternion.
		// For best results, call on a unit quaternion (or use normalized()).
		[[nodiscard]] constexpr vec<T, 3> rotate(const vec<T, 3> &v) const noexcept
		{
			// v' = v + 2*w*(q.xyz x v) + 2*(q.xyz x (q.xyz x v))
			const vec<T, 3> qv{x, y, z};
			const vec<T, 3> t = T{2} * qv.cross(v);
			return v + (w * t) + qv.cross(t);
		}

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

		[[nodiscard]] static quat from_axis_angle(const vec<T, 3> &axis, catalyst::math::radians<T> radians) noexcept
		{
			return from_axis_angle(axis, radians.count());
		}

		[[nodiscard]] static quat from_axis_angle_degrees(const vec<T, 3> &axis, catalyst::math::degrees<T> degrees) noexcept
		{
			return from_axis_angle(axis, catalyst::math::to_radians(degrees).count());
		}

		// Right-handed intrinsic ZYX (yaw around +Z, pitch around +Y, roll around +X).
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

		[[nodiscard]] static quat from_yaw_pitch_roll(catalyst::math::radians<T> yaw, catalyst::math::radians<T> pitch, catalyst::math::radians<T> roll) noexcept
		{
			return from_yaw_pitch_roll(yaw.count(), pitch.count(), roll.count());
		}

		[[nodiscard]] static quat from_yaw_pitch_roll_degrees(catalyst::math::degrees<T> yaw_deg, catalyst::math::degrees<T> pitch_deg, catalyst::math::degrees<T> roll_deg) noexcept
		{
			const auto a = yaw_pitch_roll_from_degrees(yaw_deg, pitch_deg, roll_deg);
			return from_yaw_pitch_roll(a);
		}

		[[nodiscard]] static quat from_yaw_pitch_roll(const yaw_pitch_roll<T> &a) noexcept
		{
			return from_yaw_pitch_roll(a.yaw, a.pitch, a.roll);
		}

		[[nodiscard]] static quat from_yaw_pitch_roll_degrees(T yaw_deg, T pitch_deg, T roll_deg) noexcept
		{
			const auto a = yaw_pitch_roll_from_degrees<T>(yaw_deg, pitch_deg, roll_deg);
			return from_yaw_pitch_roll(a);
		}

		// Extract right-handed intrinsic ZYX yaw/pitch/roll.
		// Note: Euler extraction has singularities (gimbal lock). When pitch is near +/- 90 degrees,
		// yaw and roll are not uniquely determined.
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

	using quatf = quat<float>;
	using quatd = quat<double>;

} // namespace catalyst::math

