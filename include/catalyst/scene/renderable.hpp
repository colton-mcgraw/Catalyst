/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The renderable component: what an entity draws, named by ids the renderer bridge resolves.
 */

#pragma once

#include <catalyst/rendering/types.hpp>
#include <catalyst/scene/bounds.hpp>

#include <cstdint>

namespace catalyst::scene
{

    /** @brief Tag type making `mesh_id` a distinct handle type. */
    struct mesh_tag
    {
    };

    /** @brief Tag type making `material_id` a distinct handle type. */
    struct material_tag
    {
    };

    /**
     * @typedef mesh_id
     * @brief Names a mesh. The scene module never looks inside it.
     * @details Built on `rendering::resource_handle` for the same reason `catalyst::resource` is: the
     * project has one handle vocabulary and a second one would only need converting. `types.hpp` is
     * constexpr-only, so this is an include-path dependency and `catalyst_scene` links nothing from
     * the rendering module. What the id refers to is the application's business until the renderer
     * bridge lands: a row in a mesh table, an asset handle's `id()`, or a GPU buffer pair.
     */
    using mesh_id = rendering::resource_handle<mesh_tag>;

    /**
     * @typedef material_id
     * @brief Names a material. Opaque to the scene module in the same way as `mesh_id`.
     */
    using material_id = rendering::resource_handle<material_tag>;

    /**
     * @struct renderable
     * @brief Makes an entity draw a mesh with a material.
     * @details `local_bounds` is in the entity's own frame; extraction transforms it by the world
     * matrix each time rather than caching a world-space box, because the cache would need its own
     * dirty tracking and the transform is three multiplies per axis. The default is a unit cube so a
     * renderable whose bounds were never set is still culled sensibly; set it to an empty `aabb` to
     * opt out of culling entirely.
     */
    struct renderable
    {
        /** @brief The mesh to draw. A null id draws nothing. */
        mesh_id mesh{};
        /** @brief The material to draw it with. Also the primary sort key within a pass. */
        material_id material{};
        /** @brief Bounds in the entity's frame. Empty means "never cull". */
        aabb local_bounds = aabb::from_center_extents({0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f});
        /** @brief Cameras whose `layer_mask` shares a bit with this one draw this renderable. */
        std::uint32_t layer_mask = 1u;
        /** @brief False hides the renderable without removing the component. */
        bool visible = true;
        /** @brief Whether shadow passes should include this renderable. */
        bool cast_shadows = true;
    };

} // namespace catalyst::scene
