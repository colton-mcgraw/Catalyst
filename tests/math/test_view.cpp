#include <catalyst/math/transform.hpp>
#include <catalyst/math/view.hpp>

#include "test_common.hpp"

namespace
{

void test_look_at_rh_eye_maps_to_origin_and_forward_is_minus_z()
{
    using catalyst::math::vec3f;

    const vec3f eye{1.0f, 2.0f, 3.0f};
    const vec3f target{1.0f, 2.0f, 2.0f}; // looking along -Z
    const vec3f up{0.0f, 1.0f, 0.0f};

    const auto v = catalyst::math::look_at_rh(eye, target, up);

    const auto eye_vs = catalyst::math::transform_point(v, eye);
    CT_REQUIRE(catalyst::tests::nearly_equal(eye_vs.x, 0.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(eye_vs.y, 0.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(eye_vs.z, 0.0f));

    const auto target_vs = catalyst::math::transform_point(v, target);
    CT_REQUIRE(catalyst::tests::nearly_equal(target_vs.x, 0.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(target_vs.y, 0.0f));
    CT_REQUIRE(target_vs.z < 0.0f);
}

void test_look_at_lh_eye_maps_to_origin_and_forward_is_plus_z()
{
    using catalyst::math::vec3f;

    const vec3f eye{1.0f, 2.0f, 3.0f};
    const vec3f target{1.0f, 2.0f, 4.0f}; // looking along +Z
    const vec3f up{0.0f, 1.0f, 0.0f};

    const auto v = catalyst::math::look_at_lh(eye, target, up);

    const auto eye_vs = catalyst::math::transform_point(v, eye);
    CT_REQUIRE(catalyst::tests::nearly_equal(eye_vs.x, 0.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(eye_vs.y, 0.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(eye_vs.z, 0.0f));

    const auto target_vs = catalyst::math::transform_point(v, target);
    CT_REQUIRE(catalyst::tests::nearly_equal(target_vs.x, 0.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(target_vs.y, 0.0f));
    CT_REQUIRE(target_vs.z > 0.0f);
}

void test_inverse_affine_round_trip_point()
{
    using catalyst::math::quatf;
    using catalyst::math::vec3f;

    const float pi = 3.14159265358979323846f;

    const vec3f t{1.0f, 2.0f, 3.0f};
    const vec3f s{2.0f, 3.0f, 4.0f};
    const quatf r = quatf::from_axis_angle(vec3f{0.0f, 1.0f, 0.0f}, pi * 0.25f).normalized();

    const auto m = catalyst::math::trs(t, r, s);
    const auto inv = catalyst::math::inverse_affine(m);

    const vec3f p{0.25f, -1.5f, 2.0f};
    const auto p2 = catalyst::math::transform_point(inv, catalyst::math::transform_point(m, p));

    CT_REQUIRE(catalyst::tests::nearly_equal(p2.x, p.x));
    CT_REQUIRE(catalyst::tests::nearly_equal(p2.y, p.y));
    CT_REQUIRE(catalyst::tests::nearly_equal(p2.z, p.z));
}

} // namespace

int main()
{
    test_look_at_rh_eye_maps_to_origin_and_forward_is_minus_z();
    test_look_at_lh_eye_maps_to_origin_and_forward_is_plus_z();
    test_inverse_affine_round_trip_point();
    return 0;
}
