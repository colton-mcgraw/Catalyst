#pragma once

#include <catalyst/math/mat.hpp>
#include <catalyst/math/vec.hpp>

#include <concepts>
#include <type_traits>

namespace catalyst::math
{

    // Right-handed look-at view matrix.
    // Assumes camera forward is -Z in view space.
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> look_at_rh(const vec<T, 3> &eye, const vec<T, 3> &target, const vec<T, 3> &up) noexcept
    {
        const vec<T, 3> f = (target - eye).normalized();
        const vec<T, 3> s = f.cross(up).normalized();
        const vec<T, 3> u = s.cross(f);

        mat<T, 4, 4> m = mat<T, 4, 4>::identity();

        // Columns are basis vectors.
        m(0, 0) = s.x;
        m(1, 0) = s.y;
        m(2, 0) = s.z;

        m(0, 1) = u.x;
        m(1, 1) = u.y;
        m(2, 1) = u.z;

        m(0, 2) = -f.x;
        m(1, 2) = -f.y;
        m(2, 2) = -f.z;

        m(0, 3) = -s.dot(eye);
        m(1, 3) = -u.dot(eye);
        m(2, 3) = f.dot(eye);

        return m;
    }

    // Left-handed look-at view matrix.
    // Assumes camera forward is +Z in view space.
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> look_at_lh(const vec<T, 3> &eye, const vec<T, 3> &target, const vec<T, 3> &up) noexcept
    {
        const vec<T, 3> z = (target - eye).normalized();
        const vec<T, 3> x = up.cross(z).normalized();
        const vec<T, 3> y = z.cross(x);

        mat<T, 4, 4> m = mat<T, 4, 4>::identity();

        m(0, 0) = x.x;
        m(1, 0) = x.y;
        m(2, 0) = x.z;

        m(0, 1) = y.x;
        m(1, 1) = y.y;
        m(2, 1) = y.z;

        m(0, 2) = z.x;
        m(1, 2) = z.y;
        m(2, 2) = z.z;

        m(0, 3) = -x.dot(eye);
        m(1, 3) = -y.dot(eye);
        m(2, 3) = -z.dot(eye);

        return m;
    }

} // namespace catalyst::math
