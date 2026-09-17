#include <catalyst/math/linear_algebra.hpp>
#include <catalyst/math/scalar.hpp>
#include <catalyst/math/transform.hpp>
#include <catalyst/scene/transform.hpp>
#include <catalyst/scene/world.hpp>

#include "test_common.hpp"

#include <numbers>

using namespace catalyst::scene;
using catalyst::tests::near;
using catalyst::tests::near_mat4;
using catalyst::tests::near_vec3;
namespace math = catalyst::math;

namespace
{
    constexpr float half_pi = std::numbers::pi_v<float> / 2.0f;

    math::vec3f apply(const math::mat4f &m, const math::vec3f &p)
    {
        return math::transform_point(m, p);
    }

    void test_compose()
    {
        transform t;
        t.position = {1.0f, 2.0f, 3.0f};
        CT_REQUIRE(near_vec3(apply(t.to_matrix(), {0.0f, 0.0f, 0.0f}), 1.0f, 2.0f, 3.0f));

        // A right-handed quarter turn about +y sends +x to -z.
        transform r;
        r.rotation = math::quatf::from_axis_angle(world_up, half_pi);
        CT_REQUIRE(near_vec3(apply(r.to_matrix(), {1.0f, 0.0f, 0.0f}), 0.0f, 0.0f, -1.0f));
        CT_REQUIRE(near_vec3(r.forward(), -1.0f, 0.0f, 0.0f));
        CT_REQUIRE(near_vec3(r.right(), 0.0f, 0.0f, -1.0f));
        CT_REQUIRE(near_vec3(r.up(), 0.0f, 1.0f, 0.0f));

        // Scale happens before rotation and translation: T * R * S.
        transform s;
        s.position = {10.0f, 0.0f, 0.0f};
        s.rotation = math::quatf::from_axis_angle(world_up, half_pi);
        s.scale = {2.0f, 1.0f, 1.0f};
        CT_REQUIRE(near_vec3(apply(s.to_matrix(), {1.0f, 0.0f, 0.0f}), 10.0f, 0.0f, -2.0f));
    }

    void test_decompose_round_trip()
    {
        transform t;
        t.position = {-4.0f, 5.5f, 0.25f};
        t.rotation = math::quatf::from_axis_angle(math::vec3f{1.0f, 2.0f, 3.0f}, 0.7f);
        t.scale = {2.0f, 3.0f, 0.5f};

        const transform back = transform::from_matrix(t.to_matrix());
        CT_REQUIRE(near_vec3(back.position, -4.0f, 5.5f, 0.25f));
        CT_REQUIRE(near_vec3(back.scale, 2.0f, 3.0f, 0.5f));
        CT_REQUIRE(near_mat4(back.to_matrix(), t.to_matrix(), 1e-3f));

        // A mirrored matrix comes back with the sign on x and a proper rotation.
        transform m;
        m.scale = {-1.0f, 1.0f, 1.0f};
        const transform mirrored = transform::from_matrix(m.to_matrix());
        CT_REQUIRE(near_vec3(mirrored.scale, -1.0f, 1.0f, 1.0f));
        CT_REQUIRE(near_mat4(mirrored.to_matrix(), m.to_matrix(), 1e-3f));
    }

    void test_look_rotation()
    {
        CT_REQUIRE(math::approx_equal(look_rotation(world_forward), math::quatf::identity(), 1e-5f));

        transform t;
        t.rotation = look_rotation({1.0f, 0.0f, 0.0f});
        CT_REQUIRE(near_vec3(t.forward(), 1.0f, 0.0f, 0.0f));
        CT_REQUIRE(near_vec3(t.up(), 0.0f, 1.0f, 0.0f));
        CT_REQUIRE(near_vec3(t.right(), 0.0f, 0.0f, 1.0f));

        // Looking straight down is degenerate against the default up and must still be a rotation.
        transform down;
        down.rotation = look_rotation({0.0f, -1.0f, 0.0f});
        CT_REQUIRE(near_vec3(down.forward(), 0.0f, -1.0f, 0.0f));
        CT_REQUIRE(near(math::magnitude(down.rotation), 1.0f));

        // looking_at agrees with math::look_at: the view built from it maps the target onto -z.
        const transform cam = looking_at({0.0f, 2.0f, 5.0f}, {0.0f, 0.0f, 0.0f});
        CT_REQUIRE(near_vec3(cam.position, 0.0f, 2.0f, 5.0f));
        const math::mat4f view = math::inverse(cam.to_matrix());
        const math::vec3f target_in_view = apply(view, {0.0f, 0.0f, 0.0f});
        CT_REQUIRE(near(target_in_view.x(), 0.0f));
        CT_REQUIRE(near(target_in_view.y(), 0.0f));
        CT_REQUIRE(target_in_view.z() < 0.0f);
        CT_REQUIRE(near_mat4(view, math::look_at(cam.position, math::vec3f{0.0f, 0.0f, 0.0f}, world_up), 1e-4f));
    }

    void test_world_propagation()
    {
        world w;
        const entity parent = w.create();
        const entity child = w.create_child(parent);
        const entity grandchild = w.create_child(child);

        w.mutable_local(parent).position = {10.0f, 0.0f, 0.0f};
        w.mutable_local(child).position = {1.0f, 0.0f, 0.0f};
        w.mutable_local(grandchild).position = {0.0f, 1.0f, 0.0f};

        // Nothing is computed until asked.
        CT_REQUIRE(near_mat4(w.world_of(grandchild), math::mat4f::identity()));
        CT_REQUIRE(w.is_transform_dirty(parent));

        w.update_transforms();
        CT_REQUIRE(!w.is_transform_dirty(parent));
        CT_REQUIRE(!w.is_transform_dirty(child));
        CT_REQUIRE(near_vec3(apply(w.world_of(child), {0.0f, 0.0f, 0.0f}), 11.0f, 0.0f, 0.0f));
        CT_REQUIRE(near_vec3(apply(w.world_of(grandchild), {0.0f, 0.0f, 0.0f}), 11.0f, 1.0f, 0.0f));

        // Editing the parent alone updates the whole subtree, even though the child is clean.
        w.mutable_local(parent).rotation = math::quatf::from_axis_angle(world_up, half_pi);
        CT_REQUIRE(w.is_transform_dirty(parent));
        CT_REQUIRE(!w.is_transform_dirty(child));
        w.update_transforms();
        CT_REQUIRE(near_vec3(apply(w.world_of(child), {0.0f, 0.0f, 0.0f}), 10.0f, 0.0f, -1.0f));
        CT_REQUIRE(near_vec3(apply(w.world_of(grandchild), {0.0f, 0.0f, 0.0f}), 10.0f, 1.0f, -1.0f));

        // set_local marks dirty too.
        transform t;
        t.position = {0.0f, 0.0f, 5.0f};
        w.set_local(parent, t);
        CT_REQUIRE(w.is_transform_dirty(parent));
        w.update_transforms();
        CT_REQUIRE(near_vec3(apply(w.world_of(child), {0.0f, 0.0f, 0.0f}), 1.0f, 0.0f, 5.0f));
    }

    void test_reparent_keeps_local()
    {
        world w;
        const entity a = w.create();
        const entity b = w.create();
        const entity c = w.create_child(a);
        w.mutable_local(a).position = {1.0f, 0.0f, 0.0f};
        w.mutable_local(b).position = {0.0f, 100.0f, 0.0f};
        w.mutable_local(c).position = {0.0f, 0.0f, 1.0f};
        w.update_transforms();
        CT_REQUIRE(near_vec3(apply(w.world_of(c), {0.0f, 0.0f, 0.0f}), 1.0f, 0.0f, 1.0f));

        w.set_parent(c, b);
        CT_REQUIRE(w.is_transform_dirty(c));
        CT_REQUIRE(near_vec3(w.local_of(c).position, 0.0f, 0.0f, 1.0f));
        w.update_transforms();
        CT_REQUIRE(near_vec3(apply(w.world_of(c), {0.0f, 0.0f, 0.0f}), 0.0f, 100.0f, 1.0f));

        // Detaching to the root drops the parent's contribution.
        w.set_parent(c, null_entity);
        w.update_transforms();
        CT_REQUIRE(near_vec3(apply(w.world_of(c), {0.0f, 0.0f, 0.0f}), 0.0f, 0.0f, 1.0f));
    }

    void test_invalid_handles_degrade()
    {
        world w;
        const transform &t = w.local_of(null_entity);
        CT_REQUIRE(t == transform{});
        w.mutable_local(null_entity).position = {5.0f, 5.0f, 5.0f};
        CT_REQUIRE(w.local_of(null_entity) == transform{});
        CT_REQUIRE(near_mat4(w.world_of(null_entity), math::mat4f::identity()));
        CT_REQUIRE(!w.is_transform_dirty(null_entity));
    }

} // namespace

int main()
{
    test_compose();
    test_decompose_round_trip();
    test_look_rotation();
    test_world_propagation();
    test_reparent_keeps_local();
    test_invalid_handles_degrade();
    return 0;
}
