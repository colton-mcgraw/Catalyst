/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief View construction, sort keys and the extraction pass declared in render_list.hpp.
 */

#include <catalyst/math/geometry.hpp>
#include <catalyst/math/linear_algebra.hpp>
#include <catalyst/math/transform.hpp>
#include <catalyst/scene/camera.hpp>
#include <catalyst/scene/render_list.hpp>

#include <algorithm>

namespace catalyst::scene
{

    void render_list::clear() noexcept
    {
        items.clear();
        lights.clear();
        stats = extract_stats{};
    }

    render_view make_view(const math::mat4f &view, const math::mat4f &projection, float near_plane,
                          float far_plane) noexcept
    {
        render_view v;
        v.view = view;
        v.projection = projection;
        v.view_projection = projection * view;
        v.frustum = frustum::from_matrix(v.view_projection);
        v.near_plane = near_plane;
        v.far_plane = far_plane;

        // The camera's world matrix is the inverse of the view; its translation is the eye and its
        // -z axis is the look direction.
        if (const auto camera_world = math::try_inverse(view))
        {
            v.eye = math::translation_part(*camera_world);
            v.forward = math::normalized(math::transform_direction(*camera_world, world_forward));
        }
        return v;
    }

    std::optional<render_view> make_view(const world &w, entity camera_entity, float viewport_aspect) noexcept
    {
        const camera *cam = w.get<camera>(camera_entity);
        if (cam == nullptr)
            return std::nullopt;

        const math::mat4f &camera_world = w.world_of(camera_entity);
        render_view v = make_view(view_matrix(camera_world), cam->projection_matrix(viewport_aspect), cam->near_plane,
                                  cam->far_plane);
        v.camera = camera_entity;
        v.layer_mask = cam->layer_mask;

        // The world matrix is at hand, so take the eye from it directly rather than from the
        // round trip through the view's inverse.
        v.eye = math::translation_part(camera_world);
        v.forward = math::normalized(math::transform_direction(camera_world, world_forward));
        return v;
    }

    std::uint64_t make_sort_key(material_id material, float normalized_depth) noexcept
    {
        constexpr std::uint64_t depth_bits = 24;
        constexpr std::uint64_t depth_max = (std::uint64_t{1} << depth_bits) - 1u;

        const float d = std::clamp(normalized_depth, 0.0f, 1.0f);
        const std::uint64_t depth = static_cast<std::uint64_t>(d * static_cast<float>(depth_max));
        const std::uint64_t material_bits = material.id() & 0xFFFF'FFFFull;
        return (material_bits << depth_bits) | depth;
    }

    void extract(const world &w, const render_view &view, const extract_params &params, render_list &out)
    {
        out.clear();
        out.view = view;

        const float depth_range = std::max(view.far_plane - view.near_plane, 1e-6f);

        w.each<renderable>(
            [&](entity e, const renderable &r)
            {
                ++out.stats.considered;
                if (!r.visible || !r.mesh || (r.layer_mask & view.layer_mask) == 0u)
                    return;

                const math::mat4f &world = w.world_of(e);
                const aabb bounds = transformed(r.local_bounds, world);

                // An empty box is the opt-out: a renderable with no bounds is drawn, never culled.
                if (params.cull && !bounds.is_empty() && !view.frustum.intersects(bounds))
                {
                    ++out.stats.culled;
                    return;
                }

                const math::vec3f anchor =
                    bounds.is_empty() ? math::vec3f{math::translation_part(world)} : bounds.center();
                const float depth = math::dot(anchor - view.eye, view.forward);

                draw_item item;
                item.source = e;
                item.mesh = r.mesh;
                item.material = r.material;
                item.world = world;
                item.bounds = bounds;
                item.depth = depth;
                item.layer_mask = r.layer_mask;
                item.cast_shadows = r.cast_shadows;
                item.sort_key = make_sort_key(r.material, (depth - view.near_plane) / depth_range);
                out.items.push_back(item);
                ++out.stats.drawn;
            });

        w.each<light>(
            [&](entity e, const light &l)
            {
                if ((l.layer_mask & view.layer_mask) == 0u)
                    return;

                const math::mat4f &world = w.world_of(e);
                light_item item;
                item.source = e;
                item.light = l;
                item.position = math::translation_part(world);
                item.direction = math::normalized(math::transform_direction(world, world_forward));
                out.lights.push_back(item);
                ++out.stats.lights;
            });

        // Stable, so items with equal keys keep pool order and two extractions of an unchanged
        // world produce identical lists. That determinism is what makes the renderer's own
        // per-frame diffing possible later.
        if (params.sort)
            std::stable_sort(out.items.begin(), out.items.end(),
                             [](const draw_item &a, const draw_item &b) { return a.sort_key < b.sort_key; });
    }

} // namespace catalyst::scene
