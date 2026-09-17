/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The local transform every entity carries, and the conventions it is expressed in.
 */

#pragma once

#include <catalyst/math/matrix.hpp>
#include <catalyst/math/quaternion.hpp>
#include <catalyst/math/vector.hpp>

namespace catalyst::scene
{

    /**
     * @name World axes
     * @brief The scene module is right-handed with +y up, and an unrotated entity looks down -z.
     * @details These match `math::look_at`, whose view space has the camera at the origin looking
     * down -z with +y up, so a camera with an identity rotation needs no correction matrix. Anything
     * that wants "forward" should rotate `world_forward` rather than assume an axis.
     * @{
     */
    inline constexpr math::vec3f world_right{1.0f, 0.0f, 0.0f};
    inline constexpr math::vec3f world_up{0.0f, 1.0f, 0.0f};
    inline constexpr math::vec3f world_forward{0.0f, 0.0f, -1.0f};
    /** @} */

    /**
     * @struct transform
     * @brief Position, rotation and scale of an entity relative to its parent.
     * @details Stored decomposed rather than as a matrix because that is how it is edited: a camera
     * controller writes `rotation`, a spawner writes `position`, an animation writes all three, and
     * none of them wants to decompose a matrix first. The composed matrix is computed on demand by
     * `to_matrix` and cached per entity by `world::update_transforms`.
     *
     * The composition order is scale, then rotation, then translation: `to_matrix` returns
     * `T * R * S`, so a point is scaled in the entity's own frame, rotated, and then moved.
     */
    struct transform
    {
        /** @brief Translation relative to the parent's frame. */
        math::vec3f position{};
        /** @brief Rotation relative to the parent's frame. Expected to be unit length. */
        math::quatf rotation = math::quatf::identity();
        /** @brief Per-axis scale applied before rotation. */
        math::vec3f scale{1.0f, 1.0f, 1.0f};

        /**
         * @brief The 4x4 affine matrix `T * R * S` this transform describes.
         * @return A row-major matrix that maps points in this entity's frame into its parent's frame.
         */
        [[nodiscard]] math::mat4f to_matrix() const noexcept;

        /**
         * @brief Decomposes an affine matrix back into position, rotation and scale.
         * @details Scale is taken from the lengths of the three basis columns, so a negative scale is
         * recovered only up to a sign: a mirrored matrix comes back with `scale.x` negated. Shear is
         * not representable and is discarded.
         * @param m The matrix to decompose. Its bottom row is assumed to be `0 0 0 1`.
         * @return The transform whose `to_matrix` best reproduces `m`.
         */
        [[nodiscard]] static transform from_matrix(const math::mat4f &m) noexcept;

        /** @brief The direction this transform's frame calls forward, in the parent's frame. */
        [[nodiscard]] math::vec3f forward() const noexcept { return rotation.rotate(world_forward); }
        /** @brief The direction this transform's frame calls right, in the parent's frame. */
        [[nodiscard]] math::vec3f right() const noexcept { return rotation.rotate(world_right); }
        /** @brief The direction this transform's frame calls up, in the parent's frame. */
        [[nodiscard]] math::vec3f up() const noexcept { return rotation.rotate(world_up); }

        [[nodiscard]] constexpr bool operator==(const transform &other) const noexcept = default;
    };

    /**
     * @brief The rotation that turns `world_forward` into `forward`, keeping `up` as close to
     * vertical as the two allow.
     * @details `forward` need not be normalised. If `forward` and `up` are parallel there is no
     * unique answer and `world_right` is used as the up hint instead, so the function never returns
     * NaN.
     * @param forward The direction the result should look along.
     * @param up The direction the result's up axis should lean towards.
     * @return A unit quaternion.
     */
    [[nodiscard]] math::quatf look_rotation(const math::vec3f &forward, const math::vec3f &up = world_up) noexcept;

    /**
     * @brief A transform at `position` rotated to face `target`, with unit scale.
     * @details The camera counterpart of `math::look_at`, which builds the *view* matrix; this
     * builds the entity's *local* transform, which is what a camera entity actually stores.
     * @param position Where the transform sits.
     * @param target The point it faces.
     * @param up The direction its up axis leans towards.
     * @return The transform, with scale `{1, 1, 1}`.
     */
    [[nodiscard]] transform looking_at(const math::vec3f &position, const math::vec3f &target,
                                       const math::vec3f &up = world_up) noexcept;

} // namespace catalyst::scene
