#pragma once

#include <catalyst/math/mat.hpp>
#include <catalyst/math/quat.hpp>
#include <catalyst/math/vec.hpp>

#include <concepts>
#include <cmath>
#include <cstddef>
#include <optional>
#include <type_traits>

namespace catalyst::math
{

    template <typename T>
    requires is_vec_scalar_v<T>
    [[nodiscard]] constexpr mat<T, 4, 4> translation(const vec<T, 3> &t) noexcept
    {
        mat<T, 4, 4> m = mat<T, 4, 4>::identity();
        m(0, 3) = t.x;
        m(1, 3) = t.y;
        m(2, 3) = t.z;
        return m;
    }

    template <typename T>
    requires is_vec_scalar_v<T>
    [[nodiscard]] constexpr mat<T, 4, 4> scale(const vec<T, 3> &s) noexcept
    {
        mat<T, 4, 4> m{};
        m(0, 0) = s.x;
        m(1, 1) = s.y;
        m(2, 2) = s.z;
        m(3, 3) = T{1};
        return m;
    }

    template <typename T>
    requires is_vec_scalar_v<T>
    [[nodiscard]] constexpr mat<T, 4, 4> scale(T s) noexcept
    {
        return scale(vec<T, 3>{s, s, s});
    }

    template <typename T>
    requires is_vec_scalar_v<T>
    [[nodiscard]] inline mat<T, 4, 4> rotation(const quat<T> &q) noexcept
    {
        return q.to_mat4();
    }

    // Compose a TRS matrix for column vectors: v' = (T * R * S) * v
    template <typename T>
    requires is_vec_scalar_v<T>
    [[nodiscard]] inline mat<T, 4, 4> trs(const vec<T, 3> &t, const quat<T> &r, const vec<T, 3> &s) noexcept
    {
        return translation(t) * rotation(r) * scale(s);
    }

    template <typename T>
    struct transform
    {
        using value_type = T;

        vec<T, 3> t{};
        quat<T> r = quat<T>::identity();
        vec<T, 3> s{T{1}, T{1}, T{1}};

        [[nodiscard]] inline mat<T, 4, 4> to_mat4() const noexcept { return trs(t, r, s); }
    };

    template <typename T>
    requires is_vec_scalar_v<T>
    [[nodiscard]] inline vec<T, 3> transform_point(const mat<T, 4, 4> &m, const vec<T, 3> &p) noexcept
    {
        const vec<T, 4> hp{p.x, p.y, p.z, T{1}};
        const vec<T, 4> out = m * hp;

        if (out.w != T{})
        {
            const T inv_w = T{1} / out.w;
            return vec<T, 3>{out.x * inv_w, out.y * inv_w, out.z * inv_w};
        }

        return vec<T, 3>{out.x, out.y, out.z};
    }

    template <typename T>
    requires is_vec_scalar_v<T>
    [[nodiscard]] inline vec<T, 3> transform_vector(const mat<T, 4, 4> &m, const vec<T, 3> &v) noexcept
    {
        const vec<T, 4> hv{v.x, v.y, v.z, T{0}};
        const vec<T, 4> out = m * hv;
        return vec<T, 3>{out.x, out.y, out.z};
    }

    // Fast inverse for affine matrices with last row = (0,0,0,1).
    // If the 3x3 linear part is singular, returns the zero matrix.
    template <std::floating_point T>
    [[nodiscard]] inline std::optional<mat<T, 4, 4>> try_inverse_affine(const mat<T, 4, 4> &m) noexcept
    {
        const T a00 = m(0, 0), a01 = m(0, 1), a02 = m(0, 2);
        const T a10 = m(1, 0), a11 = m(1, 1), a12 = m(1, 2);
        const T a20 = m(2, 0), a21 = m(2, 1), a22 = m(2, 2);

        const T c00 = (a11 * a22) - (a12 * a21);
        const T c01 = -((a10 * a22) - (a12 * a20));
        const T c02 = (a10 * a21) - (a11 * a20);
        const T c10 = -((a01 * a22) - (a02 * a21));
        const T c11 = (a00 * a22) - (a02 * a20);
        const T c12 = -((a00 * a21) - (a01 * a20));
        const T c20 = (a01 * a12) - (a02 * a11);
        const T c21 = -((a00 * a12) - (a02 * a10));
        const T c22 = (a00 * a11) - (a01 * a10);

        const T det = (a00 * c00) + (a01 * c01) + (a02 * c02);
        if (det == T{})
            return std::nullopt;

        const T inv_det = T{1} / det;

        // inv(A) = adj(A) / det. Build adj(A) from cofactors.
        mat<T, 4, 4> out{};

        // inv(i,j) = C(j,i) / det
        out(0, 0) = c00 * inv_det;
        out(0, 1) = c10 * inv_det;
        out(0, 2) = c20 * inv_det;

        out(1, 0) = c01 * inv_det;
        out(1, 1) = c11 * inv_det;
        out(1, 2) = c21 * inv_det;

        out(2, 0) = c02 * inv_det;
        out(2, 1) = c12 * inv_det;
        out(2, 2) = c22 * inv_det;

        // Translation: -inv(A) * t
        const vec<T, 3> t{m(0, 3), m(1, 3), m(2, 3)};
        const vec<T, 3> inv_t{
            -(out(0, 0) * t.x + out(0, 1) * t.y + out(0, 2) * t.z),
            -(out(1, 0) * t.x + out(1, 1) * t.y + out(1, 2) * t.z),
            -(out(2, 0) * t.x + out(2, 1) * t.y + out(2, 2) * t.z),
        };

        out(0, 3) = inv_t.x;
        out(1, 3) = inv_t.y;
        out(2, 3) = inv_t.z;
        out(3, 3) = T{1};

        return out;
    }

    // Fast inverse for affine matrices with last row = (0,0,0,1).
    // If the 3x3 linear part is singular, returns the zero matrix.
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> inverse_affine(const mat<T, 4, 4> &m) noexcept
    {
        if (auto inv = try_inverse_affine(m))
            return *inv;
        return mat<T, 4, 4>{};
    }

} // namespace catalyst::math
