#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace catalyst::ui
{

    enum class axis : std::uint8_t
    {
        x,
        y,
    };

    // Context required to resolve relative units (dp/em/rem/%/vw/vh) to pixels.
    struct resolve_context
    {
        float dpi_scale = 1.0f;     // dp -> px
        float font_px = 16.0f;      // em -> px
        float root_font_px = 16.0f; // rem -> px

        float parent_width_px = 0.0f;  // % -> px (x axis)
        float parent_height_px = 0.0f; // % -> px (y axis)

        float viewport_width_px = 0.0f;  // vw -> px
        float viewport_height_px = 0.0f; // vh -> px
    };

    // A CSS-like measurement expression: a linear combination of units.
    // This supports `calc()`-style expressions like `50% - 10px + 2em`.
    //
    // Requirements:
    // - UnitEnum must be a contiguous enum starting at 0.
    // - UnitCount should match the number of enum values.
    template <typename UnitEnum, std::size_t UnitCount, typename Rep = float> requires std::is_enum_v<UnitEnum>
    struct calc_measure
    {
        using unit_type = UnitEnum;
        using rep = Rep;

        std::array<rep, UnitCount> coeff{};
        bool is_auto = false;

        constexpr calc_measure() noexcept = default;

        [[nodiscard]] static constexpr calc_measure auto_value() noexcept
        {
            calc_measure out{};
            out.is_auto = true;
            return out;
        }

        [[nodiscard]] static constexpr calc_measure unit(UnitEnum u, rep v) noexcept
        {
            calc_measure out{};
            out.set(u, v);
            return out;
        }

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            for (std::size_t i = 0; i < UnitCount; ++i)
            {
                if (coeff[i] != rep{})
                    return false;
            }
            return true;
        }

        [[nodiscard]] constexpr rep get(UnitEnum u) const noexcept
        {
            return coeff[static_cast<std::size_t>(u)];
        }

        constexpr void set(UnitEnum u, rep v) noexcept
        {
            coeff[static_cast<std::size_t>(u)] = v;
        }

        constexpr void add(UnitEnum u, rep v) noexcept
        {
            coeff[static_cast<std::size_t>(u)] += v;
        }

        [[nodiscard]] friend constexpr calc_measure operator+(calc_measure a, const calc_measure &b) noexcept
        {
            if (a.is_auto || b.is_auto)
                return calc_measure::auto_value();
            for (std::size_t i = 0; i < UnitCount; ++i)
                a.coeff[i] += b.coeff[i];
            return a;
        }

        [[nodiscard]] friend constexpr calc_measure operator-(calc_measure a, const calc_measure &b) noexcept
        {
            if (a.is_auto || b.is_auto)
                return calc_measure::auto_value();
            for (std::size_t i = 0; i < UnitCount; ++i)
                a.coeff[i] -= b.coeff[i];
            return a;
        }

        [[nodiscard]] friend constexpr calc_measure operator-(calc_measure a) noexcept
        {
            if (a.is_auto)
                return a;
            for (std::size_t i = 0; i < UnitCount; ++i)
                a.coeff[i] = -a.coeff[i];
            return a;
        }

        [[nodiscard]] friend constexpr calc_measure operator*(calc_measure a, rep s) noexcept
        {
            if (a.is_auto)
                return a;
            for (std::size_t i = 0; i < UnitCount; ++i)
                a.coeff[i] *= s;
            return a;
        }

        [[nodiscard]] friend constexpr calc_measure operator*(rep s, calc_measure a) noexcept { return a * s; }

        [[nodiscard]] friend constexpr calc_measure operator/(calc_measure a, rep s) noexcept
        {
            if (a.is_auto)
                return a;
            for (std::size_t i = 0; i < UnitCount; ++i)
                a.coeff[i] /= s;
            return a;
        }

        constexpr calc_measure &operator+=(const calc_measure &b) noexcept { return *this = (*this + b); }
        constexpr calc_measure &operator-=(const calc_measure &b) noexcept { return *this = (*this - b); }
        constexpr calc_measure &operator*=(rep s) noexcept { return *this = (*this * s); }
        constexpr calc_measure &operator/=(rep s) noexcept { return *this = (*this / s); }
    };

    enum class length_unit : std::uint8_t
    {
        px = 0,
        dp,
        em,
        rem,
        percent,
        vw,
        vh,
        _count,
    };

    using length = calc_measure<length_unit, static_cast<std::size_t>(length_unit::_count), float>;

    [[nodiscard]] constexpr length auto_() noexcept { return length::auto_value(); }

    [[nodiscard]] constexpr length px(float v) noexcept { return length::unit(length_unit::px, v); }
    [[nodiscard]] constexpr length dp(float v) noexcept { return length::unit(length_unit::dp, v); }
    [[nodiscard]] constexpr length em(float v) noexcept { return length::unit(length_unit::em, v); }
    [[nodiscard]] constexpr length rem(float v) noexcept { return length::unit(length_unit::rem, v); }

    // `percent(50)` means 50%.
    [[nodiscard]] constexpr length percent(float v) noexcept { return length::unit(length_unit::percent, v); }

    // `vw(10)` means 10vw (10% of viewport width).
    [[nodiscard]] constexpr length vw(float v) noexcept { return length::unit(length_unit::vw, v); }

    // `vh(10)` means 10vh (10% of viewport height).
    [[nodiscard]] constexpr length vh(float v) noexcept { return length::unit(length_unit::vh, v); }

    [[nodiscard]] inline float resolve_or(const length &v, axis a, const resolve_context &ctx, float auto_px = 0.0f) noexcept
    {
        if (v.is_auto)
            return auto_px;

        const float parent_axis_px = (a == axis::x) ? ctx.parent_width_px : ctx.parent_height_px;

        float out = 0.0f;
        out += v.get(length_unit::px);
        out += v.get(length_unit::dp) * ctx.dpi_scale;
        out += v.get(length_unit::em) * ctx.font_px;
        out += v.get(length_unit::rem) * ctx.root_font_px;
        out += v.get(length_unit::percent) * (parent_axis_px / 100.0f);
        out += v.get(length_unit::vw) * (ctx.viewport_width_px / 100.0f);
        out += v.get(length_unit::vh) * (ctx.viewport_height_px / 100.0f);
        return out;
    }

} // namespace catalyst::ui