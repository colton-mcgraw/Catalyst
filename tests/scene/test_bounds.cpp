#include <catalyst/math/scalar.hpp>
#include <catalyst/math/transform.hpp>
#include <catalyst/scene/bounds.hpp>
#include <catalyst/scene/transform.hpp>

#include "test_common.hpp"

#include <numbers>

using namespace catalyst::scene;
using catalyst::tests::near;
using catalyst::tests::near_vec3;
namespace math = catalyst::math;

namespace
{
    constexpr float half_pi = std::numbers::pi_v<float> / 2.0f;

    aabb unit_cube_at(float x, float y, float z)
    {
        return aabb::from_center_extents({x, y, z}, {0.5f, 0.5f, 0.5f});
    }

    void test_aabb_basics()
    {
        aabb box;
        CT_REQUIRE(box.is_empty());
        CT_REQUIRE(!box.contains({0.0f, 0.0f, 0.0f}));
        CT_REQUIRE(!box.intersects(unit_cube_at(0.0f, 0.0f, 0.0f)));

        box.expand({1.0f, 2.0f, 3.0f});
        CT_REQUIRE(!box.is_empty());
        CT_REQUIRE(box.contains({1.0f, 2.0f, 3.0f}));
        CT_REQUIRE(near_vec3(box.size(), 0.0f, 0.0f, 0.0f));

        box.expand({-1.0f, 0.0f, 5.0f});
        CT_REQUIRE(near_vec3(box.min, -1.0f, 0.0f, 3.0f));
        CT_REQUIRE(near_vec3(box.max, 1.0f, 2.0f, 5.0f));
        CT_REQUIRE(near_vec3(box.center(), 0.0f, 1.0f, 4.0f));
        CT_REQUIRE(near_vec3(box.extents(), 1.0f, 1.0f, 1.0f));

        // Merging an empty box changes nothing; merging into an empty box copies.
        const aabb before = box;
        box.merge(aabb{});
        CT_REQUIRE(box == before);
        CT_REQUIRE(merge(aabb{}, before) == before);

        const aabb other = unit_cube_at(0.0f, 1.0f, 4.0f);
        CT_REQUIRE(box.intersects(other));
        CT_REQUIRE(!box.intersects(unit_cube_at(10.0f, 0.0f, 0.0f)));

        // Touching counts as intersecting: culling must not drop a box on the boundary.
        CT_REQUIRE(aabb::from_min_max({0, 0, 0}, {1, 1, 1}).intersects(aabb::from_min_max({1, 0, 0}, {2, 1, 1})));

        const sphere s = bounding_sphere(unit_cube_at(1.0f, 1.0f, 1.0f));
        CT_REQUIRE(near_vec3(s.center, 1.0f, 1.0f, 1.0f));
        CT_REQUIRE(near(s.radius, std::numbers::sqrt3_v<float> * 0.5f));
        CT_REQUIRE(bounding_sphere(aabb{}) == sphere{});
    }

    void test_aabb_transformed()
    {
        const aabb cube = unit_cube_at(0.0f, 0.0f, 0.0f);

        transform t;
        t.position = {5.0f, 0.0f, 0.0f};
        const aabb moved = transformed(cube, t.to_matrix());
        CT_REQUIRE(near_vec3(moved.center(), 5.0f, 0.0f, 0.0f));
        CT_REQUIRE(near_vec3(moved.size(), 1.0f, 1.0f, 1.0f));

        // A quarter turn about z sends the box [0,1]^3 to x in [-1,0], y in [0,1].
        transform r;
        r.rotation = math::quatf::from_axis_angle(math::vec3f{0.0f, 0.0f, 1.0f}, half_pi);
        const aabb rotated = transformed(aabb::from_min_max({0, 0, 0}, {1, 1, 1}), r.to_matrix());
        CT_REQUIRE(near_vec3(rotated.min, -1.0f, 0.0f, 0.0f));
        CT_REQUIRE(near_vec3(rotated.max, 0.0f, 1.0f, 1.0f));

        // 45 degrees grows the box: the result bounds the rotated box, conservatively.
        transform d;
        d.rotation = math::quatf::from_axis_angle(math::vec3f{0.0f, 0.0f, 1.0f}, half_pi / 2.0f);
        const aabb diagonal = transformed(cube, d.to_matrix());
        CT_REQUIRE(near(diagonal.size().x(), std::numbers::sqrt2_v<float>, 1e-4f));

        transform s;
        s.scale = {2.0f, 3.0f, 4.0f};
        CT_REQUIRE(near_vec3(transformed(cube, s.to_matrix()).size(), 2.0f, 3.0f, 4.0f));

        CT_REQUIRE(transformed(aabb{}, t.to_matrix()).is_empty());
    }

    void test_plane()
    {
        const plane p = plane::from_point_normal({0.0f, 2.0f, 0.0f}, {0.0f, 5.0f, 0.0f});
        CT_REQUIRE(near_vec3(p.normal, 0.0f, 1.0f, 0.0f));
        CT_REQUIRE(near(p.signed_distance({0.0f, 2.0f, 0.0f}), 0.0f));
        CT_REQUIRE(near(p.signed_distance({7.0f, 5.0f, -3.0f}), 3.0f));
        CT_REQUIRE(near(p.signed_distance({0.0f, 0.0f, 0.0f}), -2.0f));

        const plane scaled{{0.0f, 0.0f, 2.0f}, 4.0f};
        const plane n = scaled.normalized();
        CT_REQUIRE(near_vec3(n.normal, 0.0f, 0.0f, 1.0f));
        CT_REQUIRE(near(n.distance, 2.0f));
    }

    void test_frustum()
    {
        // A camera at the origin looking down -z, 90 degree vertical field of view, square aspect.
        const math::mat4f view = math::look_at(math::vec3f{0.0f, 0.0f, 0.0f}, math::vec3f{0.0f, 0.0f, -1.0f}, world_up);
        const math::mat4f projection = math::perspective(half_pi, 1.0f, 0.1f, 100.0f);
        const frustum f = frustum::from_matrix(projection * view);

        CT_REQUIRE(f.contains({0.0f, 0.0f, -5.0f}));
        CT_REQUIRE(!f.contains({0.0f, 0.0f, 5.0f}));
        CT_REQUIRE(!f.contains({0.0f, 0.0f, -200.0f}));
        CT_REQUIRE(!f.contains({0.0f, 0.0f, -0.05f}));
        // At 90 degrees the half-height at depth d is d, so (0, 4, -5) is in and (0, 6, -5) is out.
        CT_REQUIRE(f.contains({0.0f, 4.0f, -5.0f}));
        CT_REQUIRE(!f.contains({0.0f, 6.0f, -5.0f}));
        CT_REQUIRE(!f.contains({6.0f, 0.0f, -5.0f}));

        CT_REQUIRE(f.intersects(unit_cube_at(0.0f, 0.0f, -5.0f)));
        CT_REQUIRE(!f.intersects(unit_cube_at(0.0f, 0.0f, 5.0f)));
        CT_REQUIRE(!f.intersects(unit_cube_at(100.0f, 0.0f, -5.0f)));
        CT_REQUIRE(!f.intersects(unit_cube_at(0.0f, 0.0f, -200.0f)));
        // Straddling the edge is visible.
        CT_REQUIRE(f.intersects(unit_cube_at(0.0f, 5.2f, -5.0f)));
        // A box that surrounds the eye is visible.
        CT_REQUIRE(f.intersects(aabb::from_center_extents({0.0f, 0.0f, 0.0f}, {50.0f, 50.0f, 50.0f})));
        CT_REQUIRE(!f.intersects(aabb{}));

        CT_REQUIRE(f.intersects(sphere{{0.0f, 0.0f, -5.0f}, 1.0f}));
        CT_REQUIRE(!f.intersects(sphere{{0.0f, 0.0f, 5.0f}, 1.0f}));
        CT_REQUIRE(f.intersects(sphere{{0.0f, 5.5f, -5.0f}, 1.0f}));

        // The near plane is the depth-zero plane, not the -w one: with depth in [0, 1] a point
        // just in front of the near distance is out and just behind it is in.
        const plane &near_p = f[frustum::side::near_plane];
        CT_REQUIRE(near_p.signed_distance({0.0f, 0.0f, -0.11f}) > 0.0f);
        CT_REQUIRE(near_p.signed_distance({0.0f, 0.0f, -0.09f}) < 0.0f);
        const plane &far_p = f[frustum::side::far_plane];
        CT_REQUIRE(far_p.signed_distance({0.0f, 0.0f, -99.0f}) > 0.0f);
        CT_REQUIRE(far_p.signed_distance({0.0f, 0.0f, -101.0f}) < 0.0f);

        // Orthographic: a box outside the view width is out even though it is in front.
        const math::mat4f ortho = math::orthographic(4.0f, 4.0f, 0.1f, 100.0f);
        const frustum o = frustum::from_matrix(ortho * view);
        CT_REQUIRE(o.contains({1.9f, 1.9f, -50.0f}));
        CT_REQUIRE(!o.contains({2.1f, 0.0f, -50.0f}));
    }

} // namespace

int main()
{
    test_aabb_basics();
    test_aabb_transformed();
    test_plane();
    test_frustum();
    return 0;
}
