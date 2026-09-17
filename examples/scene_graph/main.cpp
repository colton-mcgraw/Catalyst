/**
 * @file main.cpp
 * @brief Builds a small 3D world with the Catalyst scene module and extracts what a camera sees.
 * @details Shows the shape of a scene program: create entities, parent them, attach components,
 * then once a frame run whatever gameplay edits transforms, call `update_transforms`, build a
 * `render_view` from a camera entity, and `extract` a `render_list` for the renderer to draw.
 *
 * There is no window and no device here, and that is the point rather than a shortcut. The scene
 * module never calls a graphics API: `extract` produces a flat, culled, sorted list that names
 * meshes and materials by id, and a separate renderer bridge resolves those ids. So the whole of a
 * frame's scene work - hierarchy, culling, sorting, layer filtering - runs and can be inspected in a
 * console program, which is also why it can be unit tested without a GPU.
 *
 * Three things are worth copying rather than skimming. The carousel's riders are *children* of the
 * spinning platform, so the gameplay code writes one rotation per frame and eight world matrices
 * fall out of `update_transforms`. The lamp light is parented to a rider, so extraction reports it
 * orbiting with no code here placing it. And `spin` is an ordinary application struct attached with
 * `world::add` - the scene module has no registration step and knows nothing about it.
 * License: MIT (see LICENSE).
 */

#include <catalyst/logging/logging.hpp>
#include <catalyst/math/scalar.hpp>
#include <catalyst/scene/scene.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

namespace logging = catalyst::logging;
namespace math = catalyst::math;
namespace scene = catalyst::scene;

using scene::entity;
using scene::world;

namespace
{

    /** @brief Names this example in the log's category column. */
    struct example_log
    {
        static constexpr const char *name = "scene_graph";
    };

    /**
     * @struct spin
     * @brief An application component: turn this entity about an axis at a constant rate.
     * @details No base class, no registration, no virtuals - `component` asks only that a type be a
     * movable object, so this struct is as much a component as `renderable` is. The first
     * `world::add<spin>` creates its pool.
     */
    struct spin
    {
        math::vec3f axis = scene::world_up;
        float radians_per_second = 0.0f;
    };

    /** @brief The mesh and material ids this example pretends an asset system handed it. */
    namespace ids
    {
        // Opaque to the scene module: it sorts by them and hands them back, and never looks inside.
        // A real program gets these from a mesh table or an asset handle's id().
        constexpr scene::mesh_id platform_mesh{1};
        constexpr scene::mesh_id rider_mesh{2};
        constexpr scene::mesh_id banner_mesh{3};

        constexpr scene::material_id wood{10};
        constexpr scene::material_id brass{11};
        constexpr scene::material_id cloth{12};
    } // namespace ids

    /** @brief Which layer bit an entity's renderable lives on. The camera below only draws layer 1. */
    namespace layers
    {
        constexpr std::uint32_t world_geometry = 1u << 0;
        constexpr std::uint32_t editor_gizmos = 1u << 1;
    } // namespace layers

    /** @brief The handles the frame loop needs to keep. Everything else is reachable through the world. */
    struct carousel
    {
        entity platform = scene::null_entity;
        entity first_rider = scene::null_entity;
        entity lamp = scene::null_entity;
        entity camera = scene::null_entity;
        entity banner = scene::null_entity;
    };

    constexpr std::size_t rider_count = 8;
    constexpr float rider_radius = 10.0f;

    /**
     * @brief Populates a world with a spinning carousel, a camera, a sun and a lamp.
     * @param w The world to fill. Assumed empty.
     * @return The handles worth keeping.
     */
    carousel build_scene(world &w)
    {
        carousel c;

        // The platform is a root. Rotating it is the only gameplay edit the loop makes; every
        // rider's world matrix follows because they are its children.
        c.platform = w.create("platform");
        w.add<spin>(c.platform, spin{scene::world_up, math::radians(45.0f)});
        scene::renderable &deck = w.add<scene::renderable>(c.platform);
        deck.mesh = ids::platform_mesh;
        deck.material = ids::wood;
        deck.local_bounds = scene::aabb::from_center_extents({0.0f, 0.0f, 0.0f}, {rider_radius, 0.25f, rider_radius});

        for (std::size_t i = 0; i < rider_count; ++i)
        {
            const float angle = math::radians(360.0f) * static_cast<float>(i) / static_cast<float>(rider_count);

            // create_child parents at creation, so there is no separate set_parent call and no frame
            // in which the rider is briefly a root at the world origin.
            const entity rider = w.create_child(c.platform);
            w.set_name(rider, "rider_" + std::to_string(i));

            // The transform is local: this is the offset from the platform's centre, not a world
            // position. The rider never learns where the platform is.
            scene::transform &local = w.mutable_local(rider);
            local.position = {rider_radius * std::sin(angle), 1.5f, rider_radius * std::cos(angle)};

            scene::renderable &r = w.add<scene::renderable>(rider);
            r.mesh = ids::rider_mesh;
            // Two materials, alternating, so the sort below has something to group. Sorting by
            // material is what saves the renderer a pipeline change per draw.
            r.material = (i % 2 == 0) ? ids::brass : ids::wood;
            r.local_bounds = scene::aabb::from_center_extents({0.0f, 0.0f, 0.0f}, {0.6f, 1.2f, 0.6f});
            r.layer_mask = layers::world_geometry;

            if (i == 0)
                c.first_rider = rider;
        }

        // A point light parented to the first rider. Nothing below places it: extraction reads the
        // entity's world matrix, so it orbits with its rider for free. This is the reason a camera
        // and a light store no position of their own.
        c.lamp = w.create_child(c.first_rider);
        w.set_name(c.lamp, "lamp");
        w.mutable_local(c.lamp).position = {0.0f, 1.5f, 0.0f};
        scene::light &lamp = w.add<scene::light>(c.lamp);
        lamp.kind = scene::light_kind::point;
        lamp.color = {1.0f, 0.85f, 0.6f};
        lamp.intensity = 4.0f;
        lamp.range = 12.0f;

        // A directional light is aimed by its entity's rotation, so looking_at is how it is pointed.
        const entity sun = w.create("sun");
        w.set_local(sun, scene::looking_at({0.0f, 20.0f, 10.0f}, {0.0f, 0.0f, 0.0f}));
        scene::light &sunlight = w.add<scene::light>(sun);
        sunlight.kind = scene::light_kind::directional;
        sunlight.color = {1.0f, 0.98f, 0.92f};
        sunlight.intensity = 2.0f;
        sunlight.cast_shadows = true;

        // A gizmo on its own layer. The camera's layer_mask excludes it, so it is built here and
        // never extracted until the mask is widened at the end of main.
        c.banner = w.create("banner");
        w.mutable_local(c.banner).position = {0.0f, 2.5f, 2.0f};
        scene::renderable &banner = w.add<scene::renderable>(c.banner);
        banner.mesh = ids::banner_mesh;
        banner.material = ids::cloth;
        banner.layer_mask = layers::editor_gizmos;

        // The camera is an entity like any other: the projection is the component, and the view
        // matrix is this entity's world matrix inverted. Parenting it to a rider would be a ride
        // camera with no further code.
        //
        // It stands inside the carousel's ring rather than back from it, so riders swing behind it
        // and out past its edges as the platform turns, and the frustum test has work to do.
        c.camera = w.create("camera");
        w.set_local(c.camera, scene::looking_at({0.0f, 3.0f, 6.0f}, {0.0f, 1.5f, 0.0f}));
        scene::camera &cam = w.add<scene::camera>(c.camera);
        cam.fov_y = math::radians(50.0f);
        cam.near_plane = 0.1f;
        cam.far_plane = 100.0f;
        cam.layer_mask = layers::world_geometry;

        return c;
    }

    /** @brief Logs the hierarchy under `e`, one line per entity, indented by depth. */
    void log_hierarchy(const world &w, entity e, int depth)
    {
        std::string indent(static_cast<std::size_t>(depth) * 2u, ' ');
        logging::info<example_log>("{}{} (index {}, generation {})", indent, w.name_of(e), e.index, e.generation);
        for (const entity child : w.children_of(e))
            log_hierarchy(w, child, depth + 1);
    }

    /**
     * @brief The gameplay half of a frame: advance every `spin` component.
     * @details An ordinary system - it visits one pool contiguously and edits transforms. Writing
     * through `mutable_local` is what marks the entity dirty; `update_transforms` then recomputes
     * that subtree and nothing else.
     */
    void advance_spins(world &w, float dt)
    {
        w.each<spin>(
            [&w, dt](entity e, spin &s)
            {
                scene::transform &local = w.mutable_local(e);
                const math::quatf step = math::quatf::from_axis_angle(s.axis, s.radians_per_second * dt);
                // Renormalised because a product of unit quaternions drifts off the unit sphere once
                // it has been accumulated a few thousand frames.
                local.rotation = math::normalized(step * local.rotation);
            });
    }

} // namespace

int main()
{
    // One console sink, and every line below reaches the terminal, coloured when the terminal
    // understands colour.
    logging::default_logger().add_sink(logging::console_sink{});

    world w;
    const carousel c = build_scene(w);

    logging::info<example_log>("Built a world of {} entities: {} renderable, {} lights, {} cameras.", w.entity_count(),
                               w.count<scene::renderable>(), w.count<scene::light>(), w.count<scene::camera>());

    logging::info<example_log>("Hierarchy:");
    for (const entity root : w.roots())
        log_hierarchy(w, root, 1);

    // Reused across frames on purpose: extract clears it and refills it, so its vectors keep their
    // capacity and a steady-state frame allocates nothing.
    scene::render_list list;

    constexpr float aspect = 16.0f / 9.0f;
    constexpr float dt = 0.25f;
    constexpr int frames = 8;

    logging::info<example_log>("Simulating {} frames at {:.2f}s, camera fov 50 deg, aspect 16:9...", frames, dt);

    for (int frame = 0; frame < frames; ++frame)
    {
        advance_spins(w, dt);

        // The only place world matrices change. Everything after this reads a cache, which is what
        // lets extraction take a const world & and run for several views at once.
        w.update_transforms();

        // Empty when the entity is invalid or carries no camera component - a destroyed camera is a
        // frame with no view, not a crash.
        const auto view = scene::make_view(w, c.camera, aspect);
        if (!view)
        {
            logging::error<example_log>("Camera entity has no camera component.");
            return 1;
        }

        scene::extract(w, *view, {}, list);

        const scene::extract_stats &stats = list.stats;
        logging::info<example_log>("frame {}: considered {}, culled {}, drawn {}, lights {}", frame, stats.considered,
                                   stats.culled, stats.drawn, stats.lights);
    }

    // The items are in sort-key order: material first, then front to back within a material. That
    // ordering is the whole reason extraction sorts rather than leaving the renderer to.
    logging::info<example_log>("Final draw list, in submission order:");
    for (const scene::draw_item &item : list.items)
    {
        logging::info<example_log>("  key {:#018x}  material {}  depth {:6.2f}  {}", item.sort_key, item.material.id(),
                                   item.depth, w.name_of(item.source));
    }

    // Nothing above moved the lamp: it is a child of a rider, which is a child of the platform, and
    // extraction resolved all three transforms into this one world position.
    for (const scene::light_item &light : list.lights)
    {
        logging::info<example_log>("  light {} at ({:6.2f}, {:6.2f}, {:6.2f}) facing ({:5.2f}, {:5.2f}, {:5.2f})",
                                   w.name_of(light.source), light.position.x(), light.position.y(), light.position.z(),
                                   light.direction.x(), light.direction.y(), light.direction.z());
    }

    // Culling is a switch, not a policy: turning it off is how a debug view draws everything, and
    // how the cull's own cost is measured against the draws it saves. The view is copied out of the
    // list first, because extract clears the list it is filling - including its view.
    const std::uint32_t drawn_with_culling = list.stats.drawn;
    const scene::render_view last_view = list.view;
    scene::extract(w, last_view, scene::extract_params{.cull = false, .sort = true}, list);
    logging::info<example_log>("With culling off: {} drawn, against {} with the frustum test on.", list.stats.drawn,
                               drawn_with_culling);

    // The banner sat on the gizmo layer all along. Widening the camera's mask is all it takes for a
    // debug overlay to appear, because the filter is a bit test in extraction rather than a separate
    // list the application maintains.
    w.get<scene::camera>(c.camera)->layer_mask = layers::world_geometry | layers::editor_gizmos;
    const auto gizmo_view = scene::make_view(w, c.camera, aspect);
    scene::extract(w, *gizmo_view, {}, list);

    bool banner_drawn = false;
    for (const scene::draw_item &item : list.items)
        banner_drawn = banner_drawn || item.source == c.banner;

    logging::info<example_log>("With the gizmo layer enabled: {} drawn, banner among them: {}.", list.stats.drawn,
                               banner_drawn);

    // Destroying an entity takes its whole subtree with it, and the handles to everything in that
    // subtree go stale. A stale handle is rejected rather than naming whatever is created in the
    // recycled slot, which is what the generation in the handle buys.
    w.destroy(c.platform);
    logging::info<example_log>("Destroyed the platform: {} entities left; first rider valid: {}, lamp valid: {}",
                               w.entity_count(), w.is_valid(c.first_rider), w.is_valid(c.lamp));

    // The camera did not move, so the view built above still stands and only the contents of the
    // world have changed. Extraction over a world missing most of its entities is the same call.
    w.update_transforms();
    scene::extract(w, *gizmo_view, {}, list);
    logging::info<example_log>("After the carousel is gone: {} drawn, {} lights.", list.stats.drawn, list.stats.lights);

    return 0;
}
