/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Bounding volume transforms and the frustum tests.
 */

#include <catalyst/math/geometry.hpp>
#include <catalyst/scene/bounds.hpp>

#include <algorithm>

namespace catalyst::scene
{

    aabb transformed(const aabb &box, const math::mat4f &m) noexcept
    {
        if (box.is_empty())
            return aabb{};

        aabb out;
        for (std::size_t i = 0; i < 3; ++i)
        {
            // Start from the translation and accumulate each axis's contribution, taking the smaller
            // product into min and the larger into max. Sign handling is what makes this work for
            // rotations that flip an axis: the products are compared, not the inputs.
            out.min[i] = m(i, 3);
            out.max[i] = m(i, 3);
            for (std::size_t j = 0; j < 3; ++j)
            {
                const float a = m(i, j) * box.min[j];
                const float b = m(i, j) * box.max[j];
                out.min[i] += std::min(a, b);
                out.max[i] += std::max(a, b);
            }
        }
        return out;
    }

    sphere bounding_sphere(const aabb &box) noexcept
    {
        if (box.is_empty())
            return sphere{};
        return sphere{box.center(), math::magnitude(box.extents())};
    }

    plane plane::from_point_normal(const math::vec3f &point, const math::vec3f &normal) noexcept
    {
        const math::vec3f n = math::normalized(normal);
        return plane{n, -math::dot(n, point)};
    }

    plane plane::normalized() const noexcept
    {
        const float length = math::magnitude(normal);
        if (!(length > 0.0f))
            return *this;
        return plane{normal / length, distance / length};
    }

    frustum frustum::from_matrix(const math::mat4f &m) noexcept
    {
        const auto row = [&m](std::size_t r) { return math::vec4f{m(r, 0), m(r, 1), m(r, 2), m(r, 3)}; };
        const auto to_plane = [](const math::vec4f &v)
        { return plane{math::vec3f{v.x(), v.y(), v.z()}, v.w()}.normalized(); };

        const math::vec4f r0 = row(0);
        const math::vec4f r1 = row(1);
        const math::vec4f r2 = row(2);
        const math::vec4f r3 = row(3);

        // Clip-space inside is -w <= x <= w, -w <= y <= w and, with depth in [0, 1], 0 <= z <= w.
        // Each inequality rearranged to "row combination dot p >= 0" is one inward-facing plane.
        // The near plane is r2 alone rather than r3 + r2 precisely because depth starts at 0.
        frustum f;
        f.planes[static_cast<std::size_t>(side::left)] = to_plane(r3 + r0);
        f.planes[static_cast<std::size_t>(side::right)] = to_plane(r3 - r0);
        f.planes[static_cast<std::size_t>(side::bottom)] = to_plane(r3 + r1);
        f.planes[static_cast<std::size_t>(side::top)] = to_plane(r3 - r1);
        f.planes[static_cast<std::size_t>(side::near_plane)] = to_plane(r2);
        f.planes[static_cast<std::size_t>(side::far_plane)] = to_plane(r3 - r2);
        return f;
    }

    bool frustum::contains(const math::vec3f &p) const noexcept
    {
        for (const plane &pl : planes)
            if (pl.signed_distance(p) < 0.0f)
                return false;
        return true;
    }

    bool frustum::intersects(const aabb &box) const noexcept
    {
        if (box.is_empty())
            return false;

        for (const plane &pl : planes)
        {
            // The corner furthest along the normal. If even that is behind the plane, the whole
            // box is.
            const math::vec3f p{
                (pl.normal.x() >= 0.0f) ? box.max.x() : box.min.x(),
                (pl.normal.y() >= 0.0f) ? box.max.y() : box.min.y(),
                (pl.normal.z() >= 0.0f) ? box.max.z() : box.min.z(),
            };
            if (pl.signed_distance(p) < 0.0f)
                return false;
        }
        return true;
    }

    bool frustum::intersects(const sphere &s) const noexcept
    {
        for (const plane &pl : planes)
            if (pl.signed_distance(s.center) < -s.radius)
                return false;
        return true;
    }

} // namespace catalyst::scene
