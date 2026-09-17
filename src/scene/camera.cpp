/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Camera projection and view matrices.
 */

#include <catalyst/math/linear_algebra.hpp>
#include <catalyst/math/transform.hpp>
#include <catalyst/scene/camera.hpp>

namespace catalyst::scene
{

    math::mat4f camera::projection_matrix(float fallback_aspect) const noexcept
    {
        float a = effective_aspect(fallback_aspect);
        if (!(a > 0.0f))
            a = 1.0f;

        if (projection == projection_kind::orthographic)
            return math::orthographic(ortho_height * a, ortho_height, near_plane, far_plane);
        return math::perspective(fov_y, a, near_plane, far_plane);
    }

    math::mat4f view_matrix(const math::mat4f &camera_world) noexcept
    {
        if (const auto inverse = math::try_inverse(camera_world))
            return *inverse;
        return math::mat4f::identity();
    }

} // namespace catalyst::scene
