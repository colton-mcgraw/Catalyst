#pragma once

#include <catalyst/math/euler.hpp>
#include <catalyst/math/vec.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <type_traits>

namespace catalyst::math
{

    template <typename T, std::size_t Rows, std::size_t Cols>
    requires is_vec_scalar_v<T>
    struct mat
    {
        static_assert(Rows > 0 && Cols > 0, "mat<T,R,C>: R and C must be > 0");

        using value_type = T;
        static constexpr std::size_t rows = Rows;
        static constexpr std::size_t cols = Cols;

        using col_type = vec<T, Rows>;
        using row_type = vec<T, Cols>;

        std::array<col_type, Cols> c{}; // column-major: c[col][row]

        constexpr mat() noexcept = default;

        // Column-major scalar fill: list is interpreted as m(0,0), m(1,0), ... m(R-1,0), m(0,1), ...
        constexpr mat(std::initializer_list<T> init) noexcept
        {
            const std::size_t count = (std::min)(init.size(), Rows * Cols);
            auto it = init.begin();
            for (std::size_t i = 0; i < count; ++i, ++it)
            {
                const std::size_t r = i % Rows;
                const std::size_t col = i / Rows;
                (*this)(r, col) = *it;
            }
            for (std::size_t i = count; i < Rows * Cols; ++i)
            {
                const std::size_t r = i % Rows;
                const std::size_t col = i / Rows;
                (*this)(r, col) = T{};
            }
        }

        [[nodiscard]] constexpr T &operator()(std::size_t r, std::size_t col) noexcept
        {
            return c[col][r];
        }

        [[nodiscard]] constexpr const T &operator()(std::size_t r, std::size_t col) const noexcept
        {
            return c[col][r];
        }

        [[nodiscard]] constexpr col_type &column(std::size_t col) noexcept { return c[col]; }
        [[nodiscard]] constexpr const col_type &column(std::size_t col) const noexcept { return c[col]; }

        [[nodiscard]] constexpr row_type row(std::size_t r) const noexcept
        {
            row_type out{};
            for (std::size_t j = 0; j < Cols; ++j)
                out[j] = (*this)(r, j);
            return out;
        }

        [[nodiscard]] static constexpr mat from_cols(const std::array<col_type, Cols> &cols) noexcept
        {
            mat out{};
            out.c = cols;
            return out;
        }

        [[nodiscard]] static constexpr mat from_rows(const std::array<row_type, Rows> &rows_in) noexcept
        {
            mat out{};
            for (std::size_t r = 0; r < Rows; ++r)
            {
                for (std::size_t j = 0; j < Cols; ++j)
                    out(r, j) = rows_in[r][j];
            }
            return out;
        }

        [[nodiscard]] static constexpr mat identity() noexcept
        {
            static_assert(Rows == Cols, "mat::identity() requires a square matrix");
            mat out{};
            for (std::size_t i = 0; i < Rows; ++i)
                out(i, i) = T{1};
            return out;
        }

        [[nodiscard]] friend constexpr mat operator+(mat a, const mat &b) noexcept
        {
            for (std::size_t j = 0; j < Cols; ++j)
                a.c[j] += b.c[j];
            return a;
        }

        [[nodiscard]] friend constexpr mat operator-(mat a, const mat &b) noexcept
        {
            for (std::size_t j = 0; j < Cols; ++j)
                a.c[j] -= b.c[j];
            return a;
        }

        [[nodiscard]] friend constexpr mat operator*(mat a, T s) noexcept
        {
            for (std::size_t j = 0; j < Cols; ++j)
                a.c[j] *= s;
            return a;
        }

        [[nodiscard]] friend constexpr mat operator*(T s, mat a) noexcept { return a * s; }

        [[nodiscard]] friend constexpr mat operator/(mat a, T s) noexcept
        {
            for (std::size_t j = 0; j < Cols; ++j)
                a.c[j] /= s;
            return a;
        }

        constexpr mat &operator+=(const mat &other) noexcept { return *this = (*this + other); }
        constexpr mat &operator-=(const mat &other) noexcept { return *this = (*this - other); }
        constexpr mat &operator*=(T s) noexcept { return *this = (*this * s); }
        constexpr mat &operator/=(T s) noexcept { return *this = (*this / s); }
    };

    template <typename T, std::size_t R, std::size_t C>
    [[nodiscard]] constexpr vec<T, R> operator*(const mat<T, R, C> &m, const vec<T, C> &v) noexcept
    {
        vec<T, R> out{};
        for (std::size_t j = 0; j < C; ++j)
            out += m.c[j] * v[j];
        return out;
    }

    template <typename T, std::size_t R, std::size_t C, std::size_t K>
    [[nodiscard]] constexpr mat<T, R, K> operator*(const mat<T, R, C> &a, const mat<T, C, K> &b) noexcept
    {
        mat<T, R, K> out{};
        for (std::size_t j = 0; j < K; ++j)
            out.c[j] = a * b.c[j];
        return out;
    }

    using mat4f = mat<float, 4, 4>;
    using mat3f = mat<float, 3, 3>;
    using mat2f = mat<float, 2, 2>;

    using mat4d = mat<double, 4, 4>;
    using mat3d = mat<double, 3, 3>;
    using mat2d = mat<double, 2, 2>;

    // Right-handed intrinsic ZYX (yaw around +Z, pitch around +Y, roll around +X).
    // R = Rz(yaw) * Ry(pitch) * Rx(roll)
    template <typename T>
    [[nodiscard]] inline mat<T, 3, 3> rotation_yaw_pitch_roll(T yaw, T pitch, T roll) noexcept
    {
        const yaw_pitch_roll<T> a{yaw, pitch, roll};
        const auto t = detail::trig_full(a);

        mat<T, 3, 3> m{};

        // Row-major form, stored via m(row,col)
        m(0, 0) = t.cy * t.cp;
        m(0, 1) = (t.cy * t.sp * t.sr) - (t.sy * t.cr);
        m(0, 2) = (t.cy * t.sp * t.cr) + (t.sy * t.sr);

        m(1, 0) = t.sy * t.cp;
        m(1, 1) = (t.sy * t.sp * t.sr) + (t.cy * t.cr);
        m(1, 2) = (t.sy * t.sp * t.cr) - (t.cy * t.sr);

        m(2, 0) = -t.sp;
        m(2, 1) = t.cp * t.sr;
        m(2, 2) = t.cp * t.cr;

        return m;
    }

    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 3, 3> rotation_yaw_pitch_roll(radians<T> yaw, radians<T> pitch, radians<T> roll) noexcept
    {
        return rotation_yaw_pitch_roll(yaw.count(), pitch.count(), roll.count());
    }

    template <typename T>
    [[nodiscard]] inline mat<T, 3, 3> rotation_yaw_pitch_roll_degrees(T yaw_deg, T pitch_deg, T roll_deg) noexcept
    {
        const auto a = yaw_pitch_roll_from_degrees<T>(yaw_deg, pitch_deg, roll_deg);
        return rotation_yaw_pitch_roll(a);
    }

    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 3, 3> rotation_yaw_pitch_roll_degrees(degrees<T> yaw_deg, degrees<T> pitch_deg, degrees<T> roll_deg) noexcept
    {
        const auto a = yaw_pitch_roll_from_degrees(yaw_deg, pitch_deg, roll_deg);
        return rotation_yaw_pitch_roll(a);
    }

    template <typename T>
    [[nodiscard]] inline mat<T, 3, 3> rotation_yaw_pitch_roll(const yaw_pitch_roll<T> &a) noexcept
    {
        return rotation_yaw_pitch_roll(a.yaw, a.pitch, a.roll);
    }

    // Extract right-handed intrinsic ZYX yaw/pitch/roll from a rotation matrix.
    // Note: Euler extraction has singularities (gimbal lock) when pitch is near +/- 90 degrees.
    template <typename T>
    [[nodiscard]] inline yaw_pitch_roll<T> to_yaw_pitch_roll(const mat<T, 3, 3> &m) noexcept
    {
        // For R = Rz(yaw) * Ry(pitch) * Rx(roll):
        // m(2,0) = -sin(pitch)
        // m(0,0) = cos(yaw)*cos(pitch)
        // m(1,0) = sin(yaw)*cos(pitch)
        // m(2,1) = cos(pitch)*sin(roll)
        // m(2,2) = cos(pitch)*cos(roll)

        const T sp = -m(2, 0);
        const T pitch = static_cast<T>(std::asin(static_cast<long double>(detail::clamp(sp, T{-1}, T{1}))));

        // If cos(pitch) is close to 0, we're near gimbal lock.
        const T cp = static_cast<T>(std::cos(static_cast<long double>(pitch)));

        T yaw{};
        T roll{};
        if (std::fabs(cp) > static_cast<T>(1e-6))
        {
            yaw = static_cast<T>(
                std::atan2(
                    static_cast<long double>(m(1, 0)),
                    static_cast<long double>(m(0, 0))));

            roll = static_cast<T>(
                std::atan2(
                    static_cast<long double>(m(2, 1)),
                    static_cast<long double>(m(2, 2))));
        }
        else
        {
            // Gimbal lock: roll is set to 0 and yaw is inferred from the remaining terms.
            yaw = static_cast<T>(
                std::atan2(
                    static_cast<long double>(-m(0, 1)),
                    static_cast<long double>(m(1, 1))));
            roll = T{};
        }

        return yaw_pitch_roll<T>{yaw, pitch, roll};
    }

} // namespace catalyst::math