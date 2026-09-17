#include <catalyst/math/transform.hpp>
#include <catalyst/scene/scene.hpp>

#include "test_common.hpp"

#include <numbers>

using namespace catalyst::scene;
using catalyst::tests::near;
using catalyst::tests::near_vec3;
namespace math = catalyst::math;

namespace
{
    constexpr float half_pi = std::numbers::pi_v<float> / 2.0f;

    entity add_box(world &w, float x, float y, float z, std::uint64_t material)
    {
        const entity e = w.create();
        w.mutable_local(e).position = {x, y, z};
        renderable &r = w.add<renderable>(e);
        r.mesh = mesh_id{1};
        r.material = material_id{material};
        return e;
    }

    entity add_camera(world &w)
    {
        const entity cam = w.create("camera");
        camera &c = w.add<camera>(cam);
        c.fov_y = half_pi;
        c.near_plane = 0.1f;
        c.far_plane = 100.0f;
        // Layer 1 only, so the layer tests below have something to exclude. The default sees all.
        c.layer_mask = 1u;
        return cam;
    }

    void test_make_view()
    {
        world w;
        const entity nobody = w.create();
        CT_REQUIRE(!make_view(w, null_entity, 1.0f).has_value());
        CT_REQUIRE(!make_view(w, nobody, 1.0f).has_value());

        const entity cam = add_camera(w);
        w.set_local(cam, looking_at({0.0f, 2.0f, 5.0f}, {0.0f, 0.0f, 0.0f}));
        w.update_transforms();

        const auto view = make_view(w, cam, 2.0f);
        CT_REQUIRE(view.has_value());
        CT_REQUIRE(view->camera == cam);
        CT_REQUIRE(near_vec3(view->eye, 0.0f, 2.0f, 5.0f));
        const math::vec3f expected = math::normalized(math::vec3f{0.0f, -2.0f, -5.0f});
        CT_REQUIRE(near_vec3(view->forward, expected.x(), expected.y(), expected.z()));
        CT_REQUIRE(near(view->near_plane, 0.1f));
        CT_REQUIRE(near(view->far_plane, 100.0f));

        // The view maps the target to the centre of the screen.
        const math::vec3f clip = math::transform_point(view->view_projection, math::vec3f{0.0f, 0.0f, 0.0f});
        CT_REQUIRE(near(clip.x(), 0.0f, 1e-3f));
        CT_REQUIRE(near(clip.y(), 0.0f, 1e-3f));
        CT_REQUIRE(view->frustum.contains({0.0f, 0.0f, 0.0f}));

        // A camera's own aspect wins over the viewport's.
        w.get<camera>(cam)->aspect = 1.0f;
        const auto square = make_view(w, cam, 2.0f);
        CT_REQUIRE(near(square->projection(0, 0), square->projection(1, 1)));
        CT_REQUIRE(!near(view->projection(0, 0), view->projection(1, 1)));

        // The matrix overload recovers the eye and forward from the view matrix alone.
        const render_view synthetic = make_view(view->view, view->projection, 0.1f, 100.0f);
        CT_REQUIRE(is_null(synthetic.camera));
        CT_REQUIRE(near_vec3(synthetic.eye, 0.0f, 2.0f, 5.0f));
        CT_REQUIRE(near_vec3(synthetic.forward, expected.x(), expected.y(), expected.z()));
    }

    void test_sort_key()
    {
        // Material dominates depth; within a material, nearer sorts first.
        CT_REQUIRE(make_sort_key(material_id{1}, 0.9f) < make_sort_key(material_id{2}, 0.1f));
        CT_REQUIRE(make_sort_key(material_id{3}, 0.1f) < make_sort_key(material_id{3}, 0.2f));
        CT_REQUIRE(make_sort_key(material_id{3}, -1.0f) == make_sort_key(material_id{3}, 0.0f));
        CT_REQUIRE(make_sort_key(material_id{3}, 2.0f) == make_sort_key(material_id{3}, 1.0f));
        // Only the low 32 bits of the material take part, and the pass byte stays clear.
        CT_REQUIRE((make_sort_key(material_id{0xFFFF'FFFFull}, 1.0f) >> 56) == 0u);
    }

    void test_extract_culls_and_sorts()
    {
        world w;
        const entity cam = add_camera(w);

        const entity near_box = add_box(w, 0.0f, 0.0f, -5.0f, 2);
        const entity far_box = add_box(w, 0.0f, 0.0f, -10.0f, 1);
        const entity behind = add_box(w, 0.0f, 0.0f, 5.0f, 1);
        const entity hidden = add_box(w, 0.0f, 0.0f, -6.0f, 1);
        w.get<renderable>(hidden)->visible = false;
        const entity meshless = add_box(w, 0.0f, 0.0f, -7.0f, 1);
        w.get<renderable>(meshless)->mesh = mesh_id{};
        const entity other_layer = add_box(w, 0.0f, 0.0f, -8.0f, 1);
        w.get<renderable>(other_layer)->layer_mask = 2u;
        // Far above the frustum: culled if it had bounds, drawn because it has none.
        const entity unbounded = add_box(w, 0.0f, 500.0f, -50.0f, 1);
        w.get<renderable>(unbounded)->local_bounds = aabb{};

        w.update_transforms();
        const auto view = make_view(w, cam, 1.0f);
        CT_REQUIRE(view.has_value());

        render_list list;
        extract(w, *view, extract_params{}, list);

        CT_REQUIRE(list.stats.considered == 7u);
        CT_REQUIRE(list.stats.culled == 1u);
        CT_REQUIRE(list.stats.drawn == 3u);
        CT_REQUIRE(list.items.size() == 3u);
        CT_REQUIRE(list.view.camera == cam);

        // Material 1 before material 2; within material 1 front to back, so the far box at depth
        // 10 precedes the unbounded one at depth 50.
        CT_REQUIRE(list.items[0].source == far_box);
        CT_REQUIRE(list.items[1].source == unbounded);
        CT_REQUIRE(list.items[2].source == near_box);
        CT_REQUIRE(near(list.items[0].depth, 10.0f));
        CT_REQUIRE(near(list.items[1].depth, 50.0f));
        CT_REQUIRE(near(list.items[2].depth, 5.0f));
        CT_REQUIRE(list.items[1].bounds.is_empty());
        CT_REQUIRE(near_vec3(list.items[2].bounds.center(), 0.0f, 0.0f, -5.0f));
        CT_REQUIRE(list.items[2].mesh == mesh_id{1});
        CT_REQUIRE(list.items[2].material == material_id{2});
        CT_REQUIRE(near_vec3(math::transform_point(list.items[2].world, math::vec3f{0, 0, 0}), 0.0f, 0.0f, -5.0f));

        // Unsorted keeps pool order; unculled keeps the box behind the camera.
        extract(w, *view, extract_params{.cull = false, .sort = false}, list);
        CT_REQUIRE(list.stats.culled == 0u);
        CT_REQUIRE(list.items.size() == 4u);
        CT_REQUIRE(list.items[0].source == near_box);
        CT_REQUIRE(list.items[1].source == far_box);
        CT_REQUIRE(list.items[2].source == behind);
        CT_REQUIRE(list.items[2].depth < 0.0f);

        // Within one material, front to back.
        world w2;
        const entity cam2 = add_camera(w2);
        const entity deep = add_box(w2, 0.0f, 0.0f, -20.0f, 7);
        const entity shallow = add_box(w2, 0.0f, 0.0f, -3.0f, 7);
        w2.update_transforms();
        extract(w2, *make_view(w2, cam2, 1.0f), extract_params{}, list);
        CT_REQUIRE(list.items.size() == 2u);
        CT_REQUIRE(list.items[0].source == shallow);
        CT_REQUIRE(list.items[1].source == deep);
    }

    void test_extract_follows_hierarchy()
    {
        world w;
        const entity cam = add_camera(w);
        const entity rig = w.create();
        w.mutable_local(rig).rotation = math::quatf::from_axis_angle(world_up, half_pi);
        const entity box = add_box(w, 5.0f, 0.0f, 0.0f, 1);
        w.set_parent(box, rig);
        w.update_transforms();

        // The rig's quarter turn puts the box at (0, 0, -5), in front of the camera.
        render_list list;
        extract(w, *make_view(w, cam, 1.0f), extract_params{}, list);
        CT_REQUIRE(list.items.size() == 1u);
        CT_REQUIRE(near_vec3(list.items[0].bounds.center(), 0.0f, 0.0f, -5.0f));

        // Turn the rig the other way and the box is behind the camera.
        w.mutable_local(rig).rotation = math::quatf::from_axis_angle(world_up, -half_pi);
        w.update_transforms();
        extract(w, *make_view(w, cam, 1.0f), extract_params{}, list);
        CT_REQUIRE(list.items.empty());
        CT_REQUIRE(list.stats.culled == 1u);
    }

    void test_extract_lights_and_layers()
    {
        world w;
        const entity cam = add_camera(w);

        const entity sun = w.create("sun");
        light &sun_light = w.add<light>(sun);
        sun_light.kind = light_kind::directional;
        w.mutable_local(sun).rotation = look_rotation({0.0f, -1.0f, 0.0f});

        const entity lamp = w.create("lamp");
        w.add<light>(lamp);
        w.mutable_local(lamp).position = {1.0f, 2.0f, 3.0f};

        const entity elsewhere = w.create();
        w.add<light>(elsewhere).layer_mask = 2u;

        const entity visible_box = add_box(w, 0.0f, 0.0f, -5.0f, 1);
        const entity layer_two_box = add_box(w, 0.0f, 0.0f, -5.0f, 1);
        w.get<renderable>(layer_two_box)->layer_mask = 2u;

        w.update_transforms();
        render_list list;
        extract(w, *make_view(w, cam, 1.0f), extract_params{}, list);

        CT_REQUIRE(list.stats.lights == 2u);
        CT_REQUIRE(list.lights.size() == 2u);
        CT_REQUIRE(list.items.size() == 1u);
        CT_REQUIRE(list.items[0].source == visible_box);

        const light_item *sun_item = nullptr;
        const light_item *lamp_item = nullptr;
        for (const light_item &item : list.lights)
        {
            if (item.source == sun)
                sun_item = &item;
            if (item.source == lamp)
                lamp_item = &item;
        }
        CT_REQUIRE(sun_item != nullptr && lamp_item != nullptr);
        CT_REQUIRE(sun_item->light.kind == light_kind::directional);
        CT_REQUIRE(near_vec3(sun_item->direction, 0.0f, -1.0f, 0.0f));
        CT_REQUIRE(near_vec3(lamp_item->position, 1.0f, 2.0f, 3.0f));
        CT_REQUIRE(near_vec3(lamp_item->direction, 0.0f, 0.0f, -1.0f));

        // Clearing keeps capacity and drops content.
        list.clear();
        CT_REQUIRE(list.items.empty());
        CT_REQUIRE(list.lights.empty());
        CT_REQUIRE(list.stats.considered == 0u);
    }

} // namespace

int main()
{
    test_make_view();
    test_sort_key();
    test_extract_culls_and_sorts();
    test_extract_follows_hierarchy();
    test_extract_lights_and_layers();
    return 0;
}
