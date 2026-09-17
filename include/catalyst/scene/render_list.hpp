/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Extraction: from a world and a view to a flat, sorted, backend-agnostic list of draws.
 */

#pragma once

#include <catalyst/math/matrix.hpp>
#include <catalyst/math/vector.hpp>
#include <catalyst/scene/bounds.hpp>
#include <catalyst/scene/entity.hpp>
#include <catalyst/scene/light.hpp>
#include <catalyst/scene/renderable.hpp>
#include <catalyst/scene/transform.hpp>
#include <catalyst/scene/world.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace catalyst::scene
{

    /**
     * @struct render_view
     * @brief Everything a pass needs to know about where it is looking from.
     * @details A view is separate from a camera entity so that a pass can be rendered from
     * somewhere no entity stands: a shadow map from a light, a reflection from a mirrored camera,
     * a thumbnail from a fixed angle. `make_view` builds one from a camera entity, and the overload
     * taking matrices builds one from anything else.
     */
    struct render_view
    {
        /** @brief The camera entity this view came from, or `null_entity` for a synthetic view. */
        entity camera = null_entity;
        /** @brief World to view space. */
        math::mat4f view = math::mat4f::identity();
        /** @brief View to clip space. */
        math::mat4f projection = math::mat4f::identity();
        /** @brief `projection * view`, cached because the frustum and every depth need it. */
        math::mat4f view_projection = math::mat4f::identity();
        /** @brief The viewpoint in world space. */
        math::vec3f eye{};
        /** @brief The unit direction the view looks along, in world space. */
        math::vec3f forward = world_forward;
        /** @brief The view volume in world space. */
        scene::frustum frustum{};
        /** @brief Near plane distance, the zero of the depth sort. */
        float near_plane = 0.1f;
        /** @brief Far plane distance, the one of the depth sort. */
        float far_plane = 1000.0f;
        /** @brief Only renderables and lights with a matching bit are extracted. */
        std::uint32_t layer_mask = 0xFFFF'FFFFu;
    };

    /**
     * @brief Builds a view from explicit matrices.
     * @details `eye` and `forward` are recovered by inverting `view`; if it is singular they are
     * left at the origin looking down -z.
     * @param view World to view space.
     * @param projection View to clip space, with depth in `[0, 1]`.
     * @param near_plane Near plane distance, for depth normalisation.
     * @param far_plane Far plane distance, for depth normalisation.
     * @return The view, with `camera` null and every layer enabled.
     */
    [[nodiscard]] render_view make_view(const math::mat4f &view, const math::mat4f &projection, float near_plane,
                                        float far_plane) noexcept;

    /**
     * @brief Builds a view from a camera entity.
     * @details Reads the entity's cached world matrix, so `world::update_transforms` must have run
     * since the camera last moved.
     * @param w The world the camera lives in.
     * @param camera_entity An entity with a `camera` component.
     * @param viewport_aspect The viewport's width over height, used when the camera's `aspect` is zero.
     * @return The view, or empty when the entity is invalid or has no `camera` component.
     */
    [[nodiscard]] std::optional<render_view> make_view(const world &w, entity camera_entity,
                                                       float viewport_aspect) noexcept;

    /**
     * @struct draw_item
     * @brief One renderable, resolved: its world matrix, its world-space bounds and its sort key.
     * @details The renderer bridge reads this and nothing else. It carries copies rather than
     * pointers into the world so a list extracted on one thread can be consumed on another while
     * the world is being edited for the next frame.
     */
    struct draw_item
    {
        /** @brief Sort key, ascending. See `make_sort_key` for the layout. */
        std::uint64_t sort_key = 0;
        /** @brief The entity this came from, for picking and debugging. */
        entity source = null_entity;
        /** @brief The mesh to draw. */
        mesh_id mesh{};
        /** @brief The material to draw it with. */
        material_id material{};
        /** @brief The entity's world matrix. */
        math::mat4f world = math::mat4f::identity();
        /** @brief The renderable's bounds in world space. Empty when the renderable opted out of culling. */
        aabb bounds{};
        /** @brief Distance along the view's forward axis, in world units. Negative is behind the eye. */
        float depth = 0.0f;
        /** @brief Copied from the renderable, for passes that filter further. */
        std::uint32_t layer_mask = 0u;
        /** @brief Copied from the renderable, for the shadow pass. */
        bool cast_shadows = true;
    };

    /**
     * @struct light_item
     * @brief One light, resolved to a world-space position and direction.
     */
    struct light_item
    {
        /** @brief The entity this came from. */
        entity source = null_entity;
        /** @brief The light's parameters, copied. */
        scene::light light{};
        /** @brief The entity's world position. Unused by directional lights. */
        math::vec3f position{};
        /** @brief The entity's world forward axis, unit length. Unused by point lights. */
        math::vec3f direction = world_forward;
    };

    /**
     * @struct extract_stats
     * @brief What one extraction did, for overlays and tests.
     */
    struct extract_stats
    {
        /** @brief Renderables looked at. */
        std::uint32_t considered = 0;
        /** @brief Renderables rejected by the frustum test. */
        std::uint32_t culled = 0;
        /** @brief Renderables that became draw items. The remainder were invisible, meshless or on another layer. */
        std::uint32_t drawn = 0;
        /** @brief Lights that became light items. */
        std::uint32_t lights = 0;
    };

    /**
     * @struct render_list
     * @brief The output of `extract`: a view, its visible draws in sorted order, and its lights.
     * @details Reused across frames -- `extract` clears it and refills it, so the vectors keep
     * their capacity and a steady-state frame allocates nothing.
     */
    struct render_list
    {
        render_view view{};
        std::vector<draw_item> items;
        std::vector<light_item> lights;
        extract_stats stats{};

        /** @brief Empties the list without releasing its storage. */
        void clear() noexcept;
    };

    /**
     * @struct extract_params
     * @brief Switches for one extraction.
     */
    struct extract_params
    {
        /** @brief Frustum-cull against the view. Off is for debugging and for measuring the cull's own cost. */
        bool cull = true;
        /** @brief Sort the items by `sort_key`. Off leaves them in pool order. */
        bool sort = true;
    };

    /**
     * @brief The sort key for a draw.
     * @details Layout, most significant first: 8 bits reserved for the pass (zero in Tier 1), the
     * low 32 bits of the material id, then 24 bits of depth. Sorting ascending therefore groups
     * draws by material, which is what minimises pipeline and descriptor changes, and within a
     * material orders them front to back, which is what lets early depth testing reject the most
     * fragments. Exposed so a custom pass can produce keys that sort consistently with the built-in
     * ones.
     * @param material The draw's material.
     * @param normalized_depth Depth as a fraction of the near-to-far range. Clamped to `[0, 1]`.
     * @return The key.
     */
    [[nodiscard]] std::uint64_t make_sort_key(material_id material, float normalized_depth) noexcept;

    /**
     * @brief Fills a render list with everything the view can see.
     * @details Visits every `renderable`, transforms its bounds into world space, culls against the
     * view's frustum, and emits a `draw_item` for the survivors; then visits every `light`. Reads
     * cached world matrices, so `world::update_transforms` must have run first. The world is not
     * modified, which is what allows extraction for several views to run on several threads at once
     * against the same world.
     * @param w The world.
     * @param view The view, from `make_view`.
     * @param params Cull and sort switches.
     * @param out The list to fill. Cleared first.
     */
    void extract(const world &w, const render_view &view, const extract_params &params, render_list &out);

} // namespace catalyst::scene
