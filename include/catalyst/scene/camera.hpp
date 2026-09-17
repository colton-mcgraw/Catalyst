/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The camera component: a projection attached to an entity whose transform is the view.
 */

#pragma once

#include <catalyst/math/matrix.hpp>
#include <catalyst/math/scalar.hpp>

#include <cstdint>

namespace catalyst::scene
{

    /**
     * @enum projection_kind
     * @brief Which projection a camera applies.
     */
    enum class projection_kind : std::uint8_t
    {
        /** @brief A perspective projection from `camera::fov_y`. */
        perspective = 0,
        /** @brief An orthographic projection `camera::ortho_height` world units tall. */
        orthographic,
    };

    /**
     * @struct camera
     * @brief The projection half of a camera. The view half is the entity's world transform.
     * @details A camera has no position of its own: it is a component on an entity, and that
     * entity's world matrix, inverted, is the view matrix. This is what lets a camera be parented to
     * a vehicle or a character's head with no special case, and it is why there is no `look_at` here:
     * use `scene::looking_at` to set the entity's transform.
     *
     * The plane fields are named `near_plane` and `far_plane` because `<windows.h>` defines `near`
     * and `far` as macros, and a public header that uses those as identifiers breaks any translation
     * unit that happens to include it after the platform headers.
     */
    struct camera
    {
        /** @brief Which projection to apply. */
        projection_kind projection = projection_kind::perspective;
        /** @brief Vertical field of view in radians. Perspective only. */
        float fov_y = math::radians(60.0f);
        /** @brief Height of the view volume in world units. Orthographic only. */
        float ortho_height = 10.0f;
        /** @brief Distance to the near clipping plane. Must be positive for a perspective camera. */
        float near_plane = 0.1f;
        /** @brief Distance to the far clipping plane. Must exceed `near_plane`. */
        float far_plane = 1000.0f;
        /**
         * @brief Width divided by height. Zero means "use the viewport's", which is what a camera
         * that fills a window wants; a fixed value is for render targets and picture-in-picture.
         */
        float aspect = 0.0f;
        /** @brief Only renderables and lights with a matching bit in their `layer_mask` are drawn by this camera. */
        std::uint32_t layer_mask = 0xFFFF'FFFFu;

        /** @brief The aspect ratio to use: `aspect` when set, otherwise `fallback`. */
        [[nodiscard]] constexpr float effective_aspect(float fallback) const noexcept
        {
            return (aspect > 0.0f) ? aspect : fallback;
        }

        /**
         * @brief The projection matrix for this camera.
         * @param fallback_aspect The viewport's aspect ratio, used when `aspect` is zero.
         * @return A row-major clip-space matrix with depth in `[0, 1]`, per `math::perspective`.
         */
        [[nodiscard]] math::mat4f projection_matrix(float fallback_aspect) const noexcept;
    };

    /**
     * @brief The view matrix of a camera whose entity has the given world matrix.
     * @details The inverse of the world matrix. If the matrix is singular (a zero scale somewhere
     * up the hierarchy) the identity is returned rather than a matrix full of NaN, so a broken
     * camera renders from the origin instead of rendering nothing with no explanation.
     * @param camera_world The camera entity's world matrix, from `world::world_of`.
     * @return The matrix mapping world space to view space.
     */
    [[nodiscard]] math::mat4f view_matrix(const math::mat4f &camera_world) noexcept;

} // namespace catalyst::scene
