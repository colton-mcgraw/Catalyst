/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Bounding volumes and the frustum test that decides what a view can see.
 */

#pragma once

#include <catalyst/math/matrix.hpp>
#include <catalyst/math/vector.hpp>

#include <array>
#include <cstdint>
#include <limits>

namespace catalyst::scene
{

    /**
     * @struct aabb
     * @brief An axis-aligned bounding box.
     * @details A default-constructed box is *empty* (`min` at +infinity, `max` at -infinity) rather
     * than a zero-sized box at the origin. That makes `merge` and `expand` start from nothing without
     * a separate "first point" case, and it makes "this renderable has no bounds" a representable
     * state that extraction can treat as "never cull" instead of "cull everything".
     */
    struct aabb
    {
        /** @brief The corner with the smallest coordinates. */
        math::vec3f min{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::infinity()};
        /** @brief The corner with the largest coordinates. */
        math::vec3f max{-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity()};

        /** @brief A box from its two corners. */
        [[nodiscard]] static constexpr aabb from_min_max(const math::vec3f &min_, const math::vec3f &max_) noexcept
        {
            return aabb{min_, max_};
        }

        /** @brief A box from its centre and half-size on each axis. */
        [[nodiscard]] static constexpr aabb from_center_extents(const math::vec3f &center,
                                                                const math::vec3f &extents) noexcept
        {
            return aabb{center - extents, center + extents};
        }

        /** @brief True when the box contains no points. Any box with `max < min` on some axis is empty. */
        [[nodiscard]] constexpr bool is_empty() const noexcept
        {
            return !(max.x() >= min.x() && max.y() >= min.y() && max.z() >= min.z());
        }

        /** @brief The midpoint of the box. Meaningless for an empty box. */
        [[nodiscard]] constexpr math::vec3f center() const noexcept { return (min + max) * 0.5f; }
        /** @brief Half the size on each axis. Meaningless for an empty box. */
        [[nodiscard]] constexpr math::vec3f extents() const noexcept { return (max - min) * 0.5f; }
        /** @brief The full size on each axis. Meaningless for an empty box. */
        [[nodiscard]] constexpr math::vec3f size() const noexcept { return max - min; }

        /** @brief Grows the box to include a point. Works on an empty box. */
        constexpr void expand(const math::vec3f &p) noexcept
        {
            for (std::size_t i = 0; i < 3; ++i)
            {
                if (p[i] < min[i])
                    min[i] = p[i];
                if (p[i] > max[i])
                    max[i] = p[i];
            }
        }

        /** @brief Grows the box to include another box. Merging an empty box is a no-op. */
        constexpr void merge(const aabb &other) noexcept
        {
            if (other.is_empty())
                return;
            expand(other.min);
            expand(other.max);
        }

        /** @brief True when `p` lies inside or on the boundary. */
        [[nodiscard]] constexpr bool contains(const math::vec3f &p) const noexcept
        {
            return p.x() >= min.x() && p.x() <= max.x() && p.y() >= min.y() && p.y() <= max.y() && p.z() >= min.z() &&
                   p.z() <= max.z();
        }

        /** @brief True when the two boxes share at least a point. Empty boxes intersect nothing. */
        [[nodiscard]] constexpr bool intersects(const aabb &other) const noexcept
        {
            return min.x() <= other.max.x() && max.x() >= other.min.x() && min.y() <= other.max.y() &&
                   max.y() >= other.min.y() && min.z() <= other.max.z() && max.z() >= other.min.z();
        }

        [[nodiscard]] constexpr bool operator==(const aabb &other) const noexcept = default;
    };

    /** @brief The smallest box containing both inputs. */
    [[nodiscard]] constexpr aabb merge(aabb a, const aabb &b) noexcept
    {
        a.merge(b);
        return a;
    }

    /**
     * @brief The axis-aligned box enclosing `box` after it has been transformed by `m`.
     * @details Uses the per-component min/max trick (Arvo, Graphics Gems) rather than transforming
     * eight corners: three multiplies per matrix entry instead of eight point transforms, and no
     * corner array. The result is conservative: it is the bounds of the transformed box, which is
     * larger than the bounds of the transformed contents whenever the rotation is not axis-aligned.
     * @param box The box in the source frame. An empty box stays empty.
     * @param m An affine matrix whose bottom row is `0 0 0 1`.
     * @return The enclosing box in the destination frame.
     */
    [[nodiscard]] aabb transformed(const aabb &box, const math::mat4f &m) noexcept;

    /**
     * @struct sphere
     * @brief A bounding sphere.
     */
    struct sphere
    {
        math::vec3f center{};
        float radius = 0.0f;

        [[nodiscard]] constexpr bool operator==(const sphere &other) const noexcept = default;
    };

    /** @brief The sphere centred on a box that just contains its corners. Empty boxes give a zero sphere. */
    [[nodiscard]] sphere bounding_sphere(const aabb &box) noexcept;

    /**
     * @struct plane
     * @brief A plane in the form `dot(normal, p) + distance = 0`.
     * @details A point with a positive `signed_distance` is on the side the normal points to. Frustum
     * planes point inwards, so "inside" is "positive on all six".
     */
    struct plane
    {
        math::vec3f normal{0.0f, 1.0f, 0.0f};
        float distance = 0.0f;

        /** @brief The plane through `point` facing along `normal`, which need not be unit length. */
        [[nodiscard]] static plane from_point_normal(const math::vec3f &point, const math::vec3f &normal) noexcept;

        /** @brief The same plane with a unit normal, so `signed_distance` is a true distance. */
        [[nodiscard]] plane normalized() const noexcept;

        /** @brief Signed distance from the plane, positive on the normal's side. */
        [[nodiscard]] constexpr float signed_distance(const math::vec3f &p) const noexcept
        {
            return normal.x() * p.x() + normal.y() * p.y() + normal.z() * p.z() + distance;
        }

        [[nodiscard]] constexpr bool operator==(const plane &other) const noexcept = default;
    };

    /**
     * @struct frustum
     * @brief The six inward-facing planes of a view volume.
     */
    struct frustum
    {
        /**
         * @brief Names the planes in `planes`, in the order they are stored.
         * @details `near_plane` and `far_plane` rather than `near` and `far` for the reason given on
         * `camera`: `<windows.h>` defines both as empty macros.
         */
        enum class side : std::uint8_t
        {
            left = 0,
            right,
            bottom,
            top,
            near_plane,
            far_plane,
        };

        /** @brief The six planes, each with a unit normal pointing into the volume. */
        std::array<plane, 6> planes{};

        /**
         * @brief Extracts the frustum from a view-projection matrix.
         * @details Gribb and Hartmann's method, applied to a row-major matrix that maps world points
         * to clip space with depth in `[0, 1]`, which is what `math::perspective` and
         * `math::orthographic` produce. The planes are normalised so the box test below can compare
         * true distances.
         * @param view_projection `projection * view`, mapping world space to clip space.
         * @return The frustum in world space.
         */
        [[nodiscard]] static frustum from_matrix(const math::mat4f &view_projection) noexcept;

        /** @brief The plane for a side. */
        [[nodiscard]] constexpr const plane &operator[](side s) const noexcept
        {
            return planes[static_cast<std::size_t>(s)];
        }

        /** @brief True when the point is inside or on every plane. */
        [[nodiscard]] bool contains(const math::vec3f &p) const noexcept;

        /**
         * @brief Conservative box test: false only when the box is entirely outside some plane.
         * @details Tests the box corner furthest along each plane's normal (the "p-vertex"). A box
         * that straddles the volume's edge without actually entering it can pass; that costs a draw
         * of something invisible, which is the right way to be wrong for a culling test. An empty
         * box is outside.
         * @param box The box, in the same space as the frustum.
         * @return True when the box may be visible.
         */
        [[nodiscard]] bool intersects(const aabb &box) const noexcept;

        /** @brief True when the sphere is not entirely outside some plane. */
        [[nodiscard]] bool intersects(const sphere &s) const noexcept;
    };

} // namespace catalyst::scene
