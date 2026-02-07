/**
 * @file rect.hpp
 * @brief Axis-aligned 2D rectangle (`rect<T>`) with common operations.
 * @details The `rect` template represents an axis-aligned rectangle defined by minimum and maximum corners. It provides utility functions for common rectangle operations such as intersection, union, containment, and area calculation. The rectangle is defined as a half-open interval [min, max), meaning it includes the min corner but excludes the max corner. This convention is convenient for pixel rectangles (e.g. Win32 client rects where right/bottom are exclusive) and for composing intersections. All operations assume axis-alignment (i.e. the rectangle edges are parallel to the coordinate axes) and do not support rotation or skewing.
 * License: CDDL-1.0 (see LICENSE).
 */
#pragma once

#include <catalyst/math/vec.hpp>

#include <algorithm>
#include <cmath>
#include <concepts>
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
     * @struct rect
     * @tparam T Scalar type for rectangle coordinates (e.g. float, double, int).
     * @brief A simple axis-aligned rectangle type defined by minimum and maximum corners. Provides utility functions for common rectangle operations such as intersection, union, containment, and area calculation.
     * @details The rectangle is defined by two 2D vectors: `min` (inclusive) and `max` (exclusive).
     * This makes the rectangle a half-open interval $[\text{min}, \text{max})$ which is convenient for pixel
     * rectangles (e.g. Win32 client rects where right/bottom are exclusive) and for composing intersections.
     * @note All operations assume axis-alignment (i.e. the rectangle edges are parallel to the coordinate axes) and do not support rotation or skewing.
     * @see vec for 2D vector operations used in rectangle calculations.
     */
    template <typename T>
        requires is_vec_scalar_v<T>
    struct rect
    {
        using value_type = T;

        vec<T, 2> min{};
        vec<T, 2> max{};

        /**
         * @fn rect
         * @brief Constructs an empty rectangle with min and max corners initialized to zero.
         */
        constexpr rect() noexcept = default;
        /**
         * @fn rect
         * @brief Constructs a rectangle from minimum and maximum corners.
         * @param min_ The minimum corner of the rectangle (inclusive).
         * @param max_ The maximum corner of the rectangle (exclusive).
         * @note The rectangle represents the half-open interval [min, max), meaning it includes the min corner but excludes the max corner. For example, if min=(0,0) and max=(10,10), the rectangle includes points where 0 <= x < 10 and 0 <= y < 10.
         */
        constexpr rect(const vec<T, 2> &min_, const vec<T, 2> &max_) noexcept : min(min_), max(max_) {}

        /**
         * @fn from_min_max
         * @brief Creates a rectangle from minimum and maximum corners.
         * @param min_ The minimum corner of the rectangle (inclusive).
         * @param max_ The maximum corner of the rectangle (exclusive).
         * @return A rectangle defined by the given min and max corners.
         * @note This is a static factory function that provides an alternative way to construct a rectangle using named parameters. It is equivalent to calling the constructor directly with the same arguments.
         */
        [[nodiscard]] static constexpr rect from_min_max(const vec<T, 2> &min_, const vec<T, 2> &max_) noexcept
        {
            return rect{min_, max_};
        }

        /**
         * @fn from_pos_size
         * @brief Creates a rectangle from a position (minimum corner) and size.
         * @param pos The position of the minimum corner of the rectangle (inclusive).
         * @param size The size of the rectangle, defined as the width (x component) and height (y component). The size is added to the position to compute the maximum corner.
         * @return A rectangle defined by the given position and size.
         * @note This is a static factory function that allows constructing a rectangle using a position and size instead of min and max corners. The maximum corner is calculated as `pos + size`, so the rectangle represents the area from `pos` to `pos + size`.
         */
        [[nodiscard]] static constexpr rect from_pos_size(const vec<T, 2> &pos, const vec<T, 2> &size) noexcept
        {
            return rect{pos, pos + size};
        }

        /**
         * @fn from_xywh
         * @brief Creates a rectangle from x/y coordinates and width/height.
         * @param x The x-coordinate of the minimum corner of the rectangle (inclusive).
         * @param y The y-coordinate of the minimum corner of the rectangle (inclusive).
         * @param w The width of the rectangle, added to x to compute the maximum x-coordinate.
         * @param h The height of the rectangle, added to y to compute the maximum y-coordinate.
         * @return A rectangle defined by the given x/y coordinates and width/height.
         * @note This is a static factory function that provides a convenient way to construct a rectangle using separate x/y coordinates for the position and width/height for the size. It internally calls `from_pos_size` with the appropriate parameters to compute the min and max corners. The resulting rectangle represents the area from (x, y) to (x + w, y + h).
         */
        [[nodiscard]] static constexpr rect from_xywh(T x, T y, T w, T h) noexcept
        {
            return from_pos_size(vec<T, 2>{x, y}, vec<T, 2>{w, h});
        }

        /**
         * @fn size
         * @brief Computes the size of the rectangle as a 2D vector (width and height).
         * @return A vec<T, 2> representing the size of the rectangle, where the x component is the width (max.x - min.x) and the y component is the height (max.y - min.y).
         * @note The size is calculated as the difference between the maximum and minimum corners. If the rectangle is valid (i.e. max > min), the size will be positive. If the rectangle is empty or invalid (i.e. max <= min), the size will have non-positive components.
         */
        [[nodiscard]] constexpr vec<T, 2> size() const noexcept { return max - min; }
        /**
         * @fn center
         * @brief Computes the center point of the rectangle.
         * @return A vec<T, 2> representing the center of the rectangle, calculated as (min + max) / 2.
         * @note The center is computed as the midpoint between the minimum and maximum corners. For example, if min=(0,0) and max=(10,10), the center will be (5,5). If the rectangle is empty or invalid (i.e. max <= min), the center will still be computed as the midpoint of min and max, which may not correspond to a meaningful point within the rectangle.
         */
        [[nodiscard]] constexpr vec<T, 2> center() const noexcept { return (min + max) * T{0.5}; }

        /**
         * @fn is_empty
         * @brief Checks if the rectangle is empty, meaning it has zero or negative area.
         * @return true if the rectangle is empty (i.e. width <= 0 or height <= 0), false otherwise.
         * @note A rectangle is considered empty if its size has non-positive components, which occurs when the maximum corner is less than or equal to the minimum corner in either dimension. For example, if min=(0,0) and max=(10,10), the rectangle is not empty. If min=(0,0) and max=(0,10), the rectangle is empty because the width is zero. If min=(0,0) and max=(-5,10), the rectangle is empty because the width is negative. Similarly for the height component. An empty rectangle does not represent a valid area in 2D space.
         */
        [[nodiscard]] constexpr bool is_empty() const noexcept
        {
            const auto s = size();
            return (s.x <= T{}) || (s.y <= T{});
        }

        /**
         * @fn area
         * @brief Computes the area of the rectangle.
         * @return The area of the rectangle, calculated as width * height.
         * @note The area is computed as the product of the width and height, which are derived from the size of the rectangle. If the rectangle is valid (i.e. max > min), the area will be positive. If the rectangle is empty or invalid (i.e. max <= min), the area will be zero or negative, which may not correspond to a meaningful area in 2D space. The return type is a floating-point type to accommodate cases where T is an integer, allowing for fractional area values when necessary.
         */
        template <typename U = T>
            requires std::floating_point<U>
        [[nodiscard]] U area() const noexcept
        {
            const auto s = size();
            return static_cast<U>(s.x) * static_cast<U>(s.y);
        }

        /**
         * @fn contains
         * @brief Checks if a given point is contained within the rectangle.
         * @param p The point to check for containment, represented as a vec<T, 2>.
         * @return true if the point is contained within the rectangle, false otherwise.
         * @note A point is contained if min.x <= p.x < max.x and min.y <= p.y < max.y.
         */
        [[nodiscard]] constexpr bool contains(const vec<T, 2> &p) const noexcept
        {
            return (p.x >= min.x) && (p.y >= min.y) && (p.x < max.x) && (p.y < max.y);
        }

        /**
         * @fn intersects
         * @brief Checks if this rectangle intersects with another rectangle.
         * @param other The other rectangle to check for intersection.
         * @return true if the rectangles intersect, false otherwise.
         * @note Intersection is defined as having positive area (touching edges/corners does not count).
         */
        [[nodiscard]] constexpr bool intersects(const rect &other) const noexcept
        {
            return !((max.x <= other.min.x) || (min.x >= other.max.x) || (max.y <= other.min.y) || (min.y >= other.max.y));
        }

        /**
         * @fn intersection
         * @brief Computes the intersection of this rectangle with another rectangle.
         * @param other The other rectangle to compute the intersection with.
         * @return A rectangle representing the intersection area. If the rectangles do not intersect, the returned rectangle will be empty.
         * @note The intersection of two rectangles can be computed by taking the maximum of the minimum corners and the minimum of the maximum corners. Specifically, the intersection rectangle will have min = max(this->min, other.min) and max = min(this->max, other.max). If the resulting rectangle is empty (i.e. max <= min), then it means the original rectangles do not intersect, and an empty rectangle is returned. For example, if rect A has min=(0,0) and max=(10,10), and rect B has min=(5,5) and max=(15,15), then the intersection will have min=(5,5) and max=(10,10). If rect C has min=(11,11) and max=(20,20), then the intersection of A and C will be empty because they do not overlap.
         */
        [[nodiscard]] constexpr rect intersection(const rect &other) const noexcept
        {
            const rect out{min.max(other.min), max.min(other.max)};
            if (out.is_empty())
                return rect{};
            return out;
        }

        /**
         * @fn unite
         * @brief Computes the union of this rectangle with another rectangle.
         * @param other The other rectangle to compute the union with.
         * @return A rectangle representing the smallest rectangle that contains both this rectangle and the other rectangle.
         * @note The union of two rectangles can be computed by taking the minimum of the minimum corners and the maximum of the maximum corners. Specifically, the union rectangle will have min = min(this->min, other.min) and max = max(this->max, other.max). This resulting rectangle will be the smallest axis-aligned rectangle that completely contains both of the original rectangles. For example, if rect A has min=(0,0) and max=(10,10), and rect B has min=(5,5) and max=(15,15), then the union will have min=(0,0) and max=(15,15). If rect C has min=(11,11) and max=(20,20), then the union of A and C will have min=(0,0) and max=(20,20).
         */
        [[nodiscard]] constexpr rect unite(const rect &other) const noexcept
        {
            return rect{min.min(other.min), max.max(other.max)};
        }

        /**
         * @fn clamp_point
         * @brief Clamps a point into the rectangle's half-open bounds.
         * @param p The point to clamp.
         * @return The clamped point.
         * @note For integral rectangles, values that would clamp to the exclusive max are clamped to (max - 1)
         * when possible. For floating-point rectangles, values are clamped to the next representable value below
         * max at runtime.
         */
        [[nodiscard]] constexpr vec<T, 2> clamp_point(const vec<T, 2> &p) const noexcept
        {
            const auto clamp_one = [](T x, T lo, T hi) noexcept -> T
            {
                if (x < lo)
                    return lo;
                if (x >= hi)
                {
                    if (hi <= lo)
                        return lo;

                    if constexpr (std::integral<T>)
                    {
                        return static_cast<T>(hi - T{1});
                    }
                    else if constexpr (std::floating_point<T>)
                    {
                        if (std::is_constant_evaluated())
                            return hi;
                        return std::nextafter(hi, lo);
                    }
                    else
                    {
                        return hi;
                    }
                }
                return x;
            };

            return vec<T, 2>{clamp_one(p.x, min.x, max.x), clamp_one(p.y, min.y, max.y)};
        }
    };

    /**
     * @typedef rectf
     * @brief A rectangle type with float coordinates.
     */
    using rectf = rect<float>;
    /**
     * @typedef rectd
     * @brief A rectangle type with double coordinates.
     */
    using rectd = rect<double>;

} // namespace catalyst::math
