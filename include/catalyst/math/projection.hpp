#pragma once

#include <catalyst/math/euler.hpp>
#include <catalyst/math/mat.hpp>

#include <concepts>
#include <cmath>
#include <cstddef>
#include <type_traits>

namespace catalyst::math
{

    enum class handedness
    {
        right,
        left,
    };

    enum class depth_range
    {
        zero_to_one,
        neg_one_to_one,
    };

    namespace detail
    {
        template <typename T>
        [[nodiscard]] inline T tan_half_angle(T angle_radians) noexcept
        {
            const long double a = static_cast<long double>(angle_radians);
            return static_cast<T>(std::tan(a * 0.5L));
        }
    } // namespace detail

    // Perspective projection matrix.
    //
    // Notes:
    // - This library uses column vectors: clip = P * vec4(x,y,z,1).
    // - For handedness::right, the usual convention is that forward is -Z in view space.
    // - For handedness::left, the usual convention is that forward is +Z in view space.
    // - z_near and z_far are positive distances, with z_far > z_near.
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_fov_y(T fov_y_radians, T aspect, T z_near, T z_far,
                                                       handedness hand = handedness::right,
                                                       depth_range depth = depth_range::zero_to_one) noexcept
    {
        mat<T, 4, 4> m{};

        const T tan_half = detail::tan_half_angle(fov_y_radians);
        const T y_scale = T{1} / tan_half;
        const T x_scale = y_scale / aspect;

        m(0, 0) = x_scale;
        m(1, 1) = y_scale;

        const T zn = z_near;
        const T zf = z_far;

        if (hand == handedness::left)
        {
            if (depth == depth_range::zero_to_one)
            {
                // Equivalent to GLM perspectiveLH_ZO
                m(2, 2) = zf / (zf - zn);
                m(3, 2) = T{1};
                m(2, 3) = -(zf * zn) / (zf - zn);
            }
            else
            {
                // Equivalent to GLM perspectiveLH_NO
                m(2, 2) = (zf + zn) / (zf - zn);
                m(3, 2) = T{1};
                m(2, 3) = -(T{2} * zf * zn) / (zf - zn);
            }
        }
        else
        {
            if (depth == depth_range::zero_to_one)
            {
                // Equivalent to GLM perspectiveRH_ZO
                m(2, 2) = zf / (zn - zf);
                m(3, 2) = T{-1};
                m(2, 3) = (zf * zn) / (zn - zf);
            }
            else
            {
                // Equivalent to GLM perspectiveRH_NO
                m(2, 2) = (zf + zn) / (zn - zf);
                m(3, 2) = T{-1};
                m(2, 3) = (T{2} * zf * zn) / (zn - zf);
            }
        }

        return m;
    }

    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_fov_y(radians<T> fov_y, T aspect, T z_near, T z_far,
                                                       handedness hand = handedness::right,
                                                       depth_range depth = depth_range::zero_to_one) noexcept
    {
        return perspective_fov_y(fov_y.count(), aspect, z_near, z_far, hand, depth);
    }

    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_fov_y_degrees(T fov_y_degrees, T aspect, T z_near, T z_far,
                                                               handedness hand = handedness::right,
                                                               depth_range depth = depth_range::zero_to_one) noexcept
    {
        return perspective_fov_y(deg_to_rad(fov_y_degrees), aspect, z_near, z_far, hand, depth);
    }

    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_fov_y_degrees(degrees<T> fov_y_degrees, T aspect, T z_near, T z_far,
                                                               handedness hand = handedness::right,
                                                               depth_range depth = depth_range::zero_to_one) noexcept
    {
        return perspective_fov_y(to_radians(fov_y_degrees).count(), aspect, z_near, z_far, hand, depth);
    }

    // Default perspective uses the library's right-handed preference and a D3D-friendly depth range (0..1), angles in radians.
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective(T fov_y_radians, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y(fov_y_radians, aspect, z_near, z_far, handedness::right, depth_range::zero_to_one);
    }

    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective(radians<T> fov_y, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y(fov_y.count(), aspect, z_near, z_far, handedness::right, depth_range::zero_to_one);
    }

    // Same as default perspective, but with fov in degrees instead of radians.
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_degrees(T fov_y_degrees, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y_degrees(fov_y_degrees, aspect, z_near, z_far, handedness::right, depth_range::zero_to_one);
    }

    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_degrees(degrees<T> fov_y_degrees, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y_degrees(fov_y_degrees, aspect, z_near, z_far, handedness::right, depth_range::zero_to_one);
    }

    // Right-handed perspective projection matrice with zero-to-one depth range.
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_rh_zo(T fov_y_radians, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y(fov_y_radians, aspect, z_near, z_far, handedness::right, depth_range::zero_to_one);
    }

    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_rh_zo(radians<T> fov_y, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y(fov_y.count(), aspect, z_near, z_far, handedness::right, depth_range::zero_to_one);
    }

    // Left-handed perspective projection matrice with zero-to-one depth range.
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_lh_zo(T fov_y_radians, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y(fov_y_radians, aspect, z_near, z_far, handedness::left, depth_range::zero_to_one);
    }

    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_lh_zo(radians<T> fov_y, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y(fov_y.count(), aspect, z_near, z_far, handedness::left, depth_range::zero_to_one);
    }

    // Right-handed perspective projection matrice with negative-one-to-one depth range.
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_rh_no(T fov_y_radians, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y(fov_y_radians, aspect, z_near, z_far, handedness::right, depth_range::neg_one_to_one);
    }

    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_rh_no(radians<T> fov_y, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y(fov_y.count(), aspect, z_near, z_far, handedness::right, depth_range::neg_one_to_one);
    }

    // Left-handed perspective projection matrice with negative-one-to-one depth range.
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_lh_no(T fov_y_radians, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y(fov_y_radians, aspect, z_near, z_far, handedness::left, depth_range::neg_one_to_one);
    }

    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_lh_no(radians<T> fov_y, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y(fov_y.count(), aspect, z_near, z_far, handedness::left, depth_range::neg_one_to_one);
    }

    // Orthographic projection matrix.
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> orthographic(T left, T right, T bottom, T top, T z_near, T z_far,
                                                  handedness hand = handedness::right,
                                                  depth_range depth = depth_range::zero_to_one) noexcept
    {
        mat<T, 4, 4> m{};

        const T rl = right - left;
        const T tb = top - bottom;

        m(0, 0) = T{2} / rl;
        m(1, 1) = T{2} / tb;
        m(0, 3) = -(right + left) / rl;
        m(1, 3) = -(top + bottom) / tb;

        const T zn = z_near;
        const T zf = z_far;

        if (hand == handedness::left)
        {
            if (depth == depth_range::zero_to_one)
            {
                // Equivalent to GLM orthoLH_ZO
                m(2, 2) = T{1} / (zf - zn);
                m(2, 3) = -zn / (zf - zn);
            }
            else
            {
                // Equivalent to GLM orthoLH_NO
                m(2, 2) = T{2} / (zf - zn);
                m(2, 3) = -(zf + zn) / (zf - zn);
            }
        }
        else
        {
            if (depth == depth_range::zero_to_one)
            {
                // Equivalent to GLM orthoRH_ZO
                m(2, 2) = T{1} / (zn - zf);
                m(2, 3) = zn / (zn - zf);
            }
            else
            {
                // Equivalent to GLM orthoRH_NO
                m(2, 2) = T{2} / (zn - zf);
                m(2, 3) = (zf + zn) / (zn - zf);
            }
        }

        m(3, 3) = T{1};
        return m;
    }

} // namespace catalyst::math