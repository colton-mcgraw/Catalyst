/**
 * @file geometry.hpp
 * @brief Defines the geometric primitives used throughout the catalyst::ui module.
 * @details The UI module reuses the vector and rectangle types from catalyst::math rather than
 * defining its own, and adds the two shapes that a box model needs but a general math library does
 * not: `edges` (a value per box side, used for margin, border and padding) and `corners` (a value
 * per box corner, used for border radii). Both are templates so the same shape can carry unresolved
 * `length` measurements in a style and resolved pixel floats in a layout result.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include <catalyst/math/rect.hpp>
#include <catalyst/math/vec.hpp>
#include <catalyst/ui/measurement.hpp>

namespace catalyst::ui
{

    /**
     * @typedef point
     * @brief A position in UI space, in pixels, with the origin at the top-left and y increasing downwards.
     */
    using point = math::vec2<float>;

    /**
     * @typedef extent
     * @brief A width/height pair in pixels. Stored in `x`/`y` respectively.
     */
    using extent = math::vec2<float>;

    /**
     * @typedef rect
     * @brief An axis-aligned rectangle in UI space, in pixels.
     * @details Uses the min/max representation from catalyst::math, so `min` is the top-left corner
     * and `max` is the bottom-right corner.
     */
    using rect = math::rect<float>;

    /**
     * @struct edges
     * @brief A value for each side of a box, ordered left, top, right, bottom.
     * @details This is the shape of a CSS margin, border width or padding. It is templated on the
     * value type so that a style can hold `edges<length>` (unresolved, e.g. "8px + 2%") while a
     * layout result holds `edges<float>` (resolved pixels).
     * @tparam T The per-side value type.
     */
    template <typename T>
    struct edges
    {
        /**
         * @brief The value on the left side of the box.
         */
        T left{};
        /**
         * @brief The value on the top side of the box.
         */
        T top{};
        /**
         * @brief The value on the right side of the box.
         */
        T right{};
        /**
         * @brief The value on the bottom side of the box.
         */
        T bottom{};

        /**
         * @fn all
         * @brief Builds an `edges` with the same value on every side.
         * @param v The value to apply to all four sides.
         * @return An `edges` whose four sides are all `v`.
         */
        [[nodiscard]] static constexpr edges all(const T &v) noexcept { return edges{v, v, v, v}; }

        /**
         * @fn symmetric
         * @brief Builds an `edges` from a horizontal and a vertical value.
         * @param horizontal The value applied to the left and right sides.
         * @param vertical The value applied to the top and bottom sides.
         * @return An `edges` with the given horizontal and vertical values.
         */
        [[nodiscard]] static constexpr edges symmetric(const T &horizontal, const T &vertical) noexcept
        {
            return edges{horizontal, vertical, horizontal, vertical};
        }

        /**
         * @fn horizontal
         * @brief Returns the combined left and right values, i.e. the total size this `edges` adds along the x axis.
         * @return The sum of the left and right values.
         */
        [[nodiscard]] constexpr T horizontal() const noexcept { return left + right; }

        /**
         * @fn vertical
         * @brief Returns the combined top and bottom values, i.e. the total size this `edges` adds along the y axis.
         * @return The sum of the top and bottom values.
         */
        [[nodiscard]] constexpr T vertical() const noexcept { return top + bottom; }

        /**
         * @fn along
         * @brief Returns the combined values on the given axis.
         * @param a The axis to sum along: `axis::x` sums left and right, `axis::y` sums top and bottom.
         * @return The sum of the two sides on that axis.
         */
        [[nodiscard]] constexpr T along(axis a) const noexcept { return (a == axis::x) ? horizontal() : vertical(); }

        /**
         * @fn start
         * @brief Returns the value on the leading side of the given axis (left for x, top for y).
         * @param a The axis to read.
         * @return The leading-side value for that axis.
         */
        [[nodiscard]] constexpr T start(axis a) const noexcept { return (a == axis::x) ? left : top; }

        /**
         * @fn end
         * @brief Returns the value on the trailing side of the given axis (right for x, bottom for y).
         * @param a The axis to read.
         * @return The trailing-side value for that axis.
         */
        [[nodiscard]] constexpr T end(axis a) const noexcept { return (a == axis::x) ? right : bottom; }

        /**
         * @brief Adds two `edges` side by side.
         */
        [[nodiscard]] constexpr edges operator+(const edges &other) const noexcept
        {
            return edges{left + other.left, top + other.top, right + other.right, bottom + other.bottom};
        }

        /**
         * @brief Compares two `edges` for equality, side by side.
         */
        [[nodiscard]] constexpr bool operator==(const edges &other) const noexcept = default;
    };

    /**
     * @typedef edges_px
     * @brief Per-side values already resolved to pixels, as produced by the layout engine.
     */
    using edges_px = edges<float>;

    /**
     * @typedef edges_length
     * @brief Per-side values as unresolved measurements, as authored in a style.
     */
    using edges_length = edges<length>;

    /**
     * @struct corners
     * @brief A value for each corner of a box, ordered top-left, top-right, bottom-right, bottom-left.
     * @details This is the shape of a CSS border radius. Like `edges`, it is templated so it can hold
     * either unresolved measurements or resolved pixels.
     * @tparam T The per-corner value type.
     */
    template <typename T>
    struct corners
    {
        /**
         * @brief The value at the top-left corner of the box.
         */
        T top_left{};
        /**
         * @brief The value at the top-right corner of the box.
         */
        T top_right{};
        /**
         * @brief The value at the bottom-right corner of the box.
         */
        T bottom_right{};
        /**
         * @brief The value at the bottom-left corner of the box.
         */
        T bottom_left{};

        /**
         * @fn all
         * @brief Builds a `corners` with the same value at every corner.
         * @param v The value to apply to all four corners.
         * @return A `corners` whose four corners are all `v`.
         */
        [[nodiscard]] static constexpr corners all(const T &v) noexcept { return corners{v, v, v, v}; }

        /**
         * @brief Compares two `corners` for equality, corner by corner.
         */
        [[nodiscard]] constexpr bool operator==(const corners &other) const noexcept = default;
    };

    /**
     * @typedef corners_px
     * @brief Per-corner values already resolved to pixels.
     */
    using corners_px = corners<float>;

    /**
     * @typedef corners_length
     * @brief Per-corner values as unresolved measurements, as authored in a style.
     */
    using corners_length = corners<length>;

    /**
     * @fn deflate
     * @brief Shrinks a rectangle inwards by the given per-side amounts.
     * @details Used to step from one box to the next in the box model, e.g. from a border box to a
     * padding box. If the edges are larger than the rectangle, the result is collapsed to a
     * zero-sized rectangle at the top-left of what remains rather than being allowed to invert.
     * @param r The rectangle to shrink.
     * @param e The per-side amounts to remove, in pixels.
     * @return The shrunken rectangle.
     */
    [[nodiscard]] constexpr rect deflate(const rect &r, const edges_px &e) noexcept
    {
        const float min_x = r.min.x + e.left;
        const float min_y = r.min.y + e.top;
        const float max_x = r.max.x - e.right;
        const float max_y = r.max.y - e.bottom;

        return rect{point{min_x, min_y}, point{(max_x < min_x) ? min_x : max_x, (max_y < min_y) ? min_y : max_y}};
    }

    /**
     * @fn inflate
     * @brief Grows a rectangle outwards by the given per-side amounts.
     * @details The inverse of `deflate`, used to step outwards in the box model, e.g. from a border
     * box to a margin box.
     * @param r The rectangle to grow.
     * @param e The per-side amounts to add, in pixels.
     * @return The grown rectangle.
     */
    [[nodiscard]] constexpr rect inflate(const rect &r, const edges_px &e) noexcept
    {
        return rect{point{r.min.x - e.left, r.min.y - e.top}, point{r.max.x + e.right, r.max.y + e.bottom}};
    }

    /**
     * @fn resolve
     * @brief Resolves an `edges_length` to pixels.
     * @details Follows the CSS rule that percentages on all four sides of a margin or padding resolve
     * against the containing block's *width*, so that a symmetric percentage padding stays symmetric
     * regardless of the box's aspect ratio. The top and bottom sides are therefore resolved on
     * `axis::x` as well.
     * @param e The per-side measurements to resolve.
     * @param ctx The context supplying DPI, font sizes, parent dimensions and viewport dimensions.
     * @return The per-side values in pixels, with `auto` sides resolving to zero.
     */
    [[nodiscard]] inline edges_px resolve(const edges_length &e, const resolve_context &ctx) noexcept
    {
        return edges_px{
            resolve_or(e.left, axis::x, ctx, 0.0f),
            resolve_or(e.top, axis::x, ctx, 0.0f),
            resolve_or(e.right, axis::x, ctx, 0.0f),
            resolve_or(e.bottom, axis::x, ctx, 0.0f),
        };
    }

} // namespace catalyst::ui
