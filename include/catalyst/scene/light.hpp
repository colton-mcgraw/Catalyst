/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The light component.
 */

#pragma once

#include <catalyst/math/scalar.hpp>
#include <catalyst/math/vector.hpp>

#include <cstdint>

namespace catalyst::scene
{

    /**
     * @enum light_kind
     * @brief The shape of a light's emission.
     */
    enum class light_kind : std::uint8_t
    {
        /** @brief Parallel light from infinitely far away, along the entity's forward axis. Position is ignored. */
        directional = 0,
        /** @brief Light radiating equally in all directions from the entity's position. */
        point,
        /** @brief A cone of light from the entity's position along its forward axis. */
        spot,
    };

    /**
     * @struct light
     * @brief A light source attached to an entity. Position and direction come from the entity's transform.
     * @details Like `camera`, the light stores only what the transform cannot: extraction reads the
     * entity's world matrix to place it, so a spotlight parented to a turret turns with it for free.
     * Units are deliberately unspecified in Tier 1 -- `intensity` is whatever the renderer's shading
     * model multiplies by -- because choosing lumens versus radiance is a renderer decision.
     */
    struct light
    {
        /** @brief The emission shape. */
        light_kind kind = light_kind::point;
        /** @brief Linear RGB colour of the emitted light. */
        math::vec3f color{1.0f, 1.0f, 1.0f};
        /** @brief Scalar multiplier on `color`. */
        float intensity = 1.0f;
        /** @brief Distance beyond which a point or spot light contributes nothing. Zero means unbounded. */
        float range = 10.0f;
        /** @brief Half-angle in radians inside which a spot light is at full intensity. */
        float inner_cone = math::radians(20.0f);
        /** @brief Half-angle in radians outside which a spot light contributes nothing. */
        float outer_cone = math::radians(30.0f);
        /** @brief Whether the renderer should render a shadow map for this light. */
        bool cast_shadows = false;
        /** @brief Cameras whose `layer_mask` shares a bit with this one see this light. */
        std::uint32_t layer_mask = 0xFFFF'FFFFu;
    };

} // namespace catalyst::scene
