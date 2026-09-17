/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Composition and decomposition of `scene::transform`.
 */

#include <catalyst/math/geometry.hpp>
#include <catalyst/math/linear_algebra.hpp>
#include <catalyst/math/transform.hpp>
#include <catalyst/scene/transform.hpp>

#include <cmath>

namespace catalyst::scene
{

    math::mat4f transform::to_matrix() const noexcept
    {
        const math::mat3f linear = rotation.to_matrix() * math::scaling(scale);
        return math::affine(linear, position);
    }

    transform transform::from_matrix(const math::mat4f &m) noexcept
    {
        transform t;
        t.position = math::vec3f{m(0, 3), m(1, 3), m(2, 3)};

        // The basis vectors are the columns of the linear part, because `transform_point` computes
        // result_i = sum_k m(i, k) * p_k: column k is where the k-th axis lands.
        const math::vec3f c0{m(0, 0), m(1, 0), m(2, 0)};
        const math::vec3f c1{m(0, 1), m(1, 1), m(2, 1)};
        const math::vec3f c2{m(0, 2), m(1, 2), m(2, 2)};

        float sx = math::magnitude(c0);
        const float sy = math::magnitude(c1);
        const float sz = math::magnitude(c2);

        // A mirrored matrix has a negative determinant. Put the sign on one axis so the remaining
        // rotation is proper; without this, from_rotation_matrix would be handed an improper matrix
        // and return a quaternion that rotates by the wrong amount around the wrong axis.
        if (math::determinant(math::linear_part(m)) < 0.0f)
            sx = -sx;

        t.scale = math::vec3f{sx, sy, sz};

        // A collapsed axis has no direction to recover the rotation from.
        if (sx == 0.0f || sy == 0.0f || sz == 0.0f)
            return t;

        const math::mat3f r = math::mat3f::from_rows({
            {c0.x() / sx, c1.x() / sy, c2.x() / sz},
            {c0.y() / sx, c1.y() / sy, c2.y() / sz},
            {c0.z() / sx, c1.z() / sy, c2.z() / sz},
        });
        t.rotation = math::normalized(math::quatf::from_rotation_matrix(r));
        return t;
    }

    math::quatf look_rotation(const math::vec3f &forward, const math::vec3f &up) noexcept
    {
        const float length = math::magnitude(forward);
        if (!(length > 0.0f))
            return math::quatf::identity();

        const math::vec3f f = forward / length;
        math::vec3f s = math::cross(f, up);

        // Looking straight along `up` leaves no unique right vector; any perpendicular will do, and
        // `world_right` cannot also be parallel to `f` when `up` is.
        if (math::magnitude_squared(s) < 1e-12f)
            s = math::cross(f, world_right);
        if (math::magnitude_squared(s) < 1e-12f)
            s = math::cross(f, world_up);

        s = math::normalized(s);
        const math::vec3f u = math::cross(s, f);

        // Columns are the frame's axes expressed in the parent: right, up, and back. Back, not
        // forward, because the frame's own -z is what it looks along.
        const math::mat3f r = math::mat3f::from_rows({
            {s.x(), u.x(), -f.x()},
            {s.y(), u.y(), -f.y()},
            {s.z(), u.z(), -f.z()},
        });
        return math::normalized(math::quatf::from_rotation_matrix(r));
    }

    transform looking_at(const math::vec3f &position, const math::vec3f &target, const math::vec3f &up) noexcept
    {
        transform t;
        t.position = position;
        t.rotation = look_rotation(target - position, up);
        return t;
    }

} // namespace catalyst::scene
