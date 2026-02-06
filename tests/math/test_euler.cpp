#include <catalyst/math/euler.hpp>
#include <catalyst/math/mat.hpp>
#include <catalyst/math/quat.hpp>

#include "test_common.hpp"

namespace
{

void test_euler_quat_vs_mat_rotation_match()
{
    using catalyst::math::quatf;
    using catalyst::math::vec3f;
    using catalyst::math::yaw_pitch_roll;

    const float pi = 3.14159265358979323846f;

    const yaw_pitch_roll<float> a{pi * 0.25f, pi * 0.10f, pi * 0.50f};

    const auto q = quatf::from_yaw_pitch_roll(a).normalized();
    const auto m = catalyst::math::rotation_yaw_pitch_roll(a);

    const vec3f v{0.25f, -1.5f, 2.0f};

    const auto vq = q.rotate(v);
    const auto vm = m * v;

    CT_REQUIRE(catalyst::tests::nearly_equal(vq.x, vm.x));
    CT_REQUIRE(catalyst::tests::nearly_equal(vq.y, vm.y));
    CT_REQUIRE(catalyst::tests::nearly_equal(vq.z, vm.z));
}

void test_euler_known_yaw_90_z()
{
    using catalyst::math::quatf;
    using catalyst::math::vec3f;

    const float pi = 3.14159265358979323846f;

    const auto q = quatf::from_yaw_pitch_roll(pi * 0.5f, 0.0f, 0.0f);
    const auto m = catalyst::math::rotation_yaw_pitch_roll(pi * 0.5f, 0.0f, 0.0f);

    const vec3f x{1.0f, 0.0f, 0.0f};

    const auto qx = q.rotate(x);
    const auto mx = m * x;

    CT_REQUIRE(catalyst::tests::nearly_equal(qx.x, 0.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(qx.y, 1.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(qx.z, 0.0f));

    CT_REQUIRE(catalyst::tests::nearly_equal(mx.x, qx.x));
    CT_REQUIRE(catalyst::tests::nearly_equal(mx.y, qx.y));
    CT_REQUIRE(catalyst::tests::nearly_equal(mx.z, qx.z));
}

void test_euler_from_degrees_helpers()
{
    using catalyst::math::quatf;
    using catalyst::math::vec3f;

    const auto q = quatf::from_yaw_pitch_roll_degrees(90.0f, 0.0f, 0.0f).normalized();
    const auto m = catalyst::math::rotation_yaw_pitch_roll_degrees(90.0f, 0.0f, 0.0f);

    const vec3f x{1.0f, 0.0f, 0.0f};
    const auto qx = q.rotate(x);
    const auto mx = m * x;

    CT_REQUIRE(catalyst::tests::nearly_equal(qx.x, 0.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(qx.y, 1.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(qx.z, 0.0f));

    CT_REQUIRE(catalyst::tests::nearly_equal(mx.x, qx.x));
    CT_REQUIRE(catalyst::tests::nearly_equal(mx.y, qx.y));
    CT_REQUIRE(catalyst::tests::nearly_equal(mx.z, qx.z));
}

void test_euler_extraction_round_trip_matches_rotation()
{
    using catalyst::math::quatf;
    using catalyst::math::vec3f;

    // Avoid gimbal lock for this round-trip test.
    const auto a = catalyst::math::yaw_pitch_roll_from_degrees<float>(30.0f, -20.0f, 10.0f);

    const auto q0 = quatf::from_yaw_pitch_roll(a).normalized();
    const auto m0 = catalyst::math::rotation_yaw_pitch_roll(a);

    const auto aq = q0.to_yaw_pitch_roll();
    const auto am = catalyst::math::to_yaw_pitch_roll(m0);

    const auto q1 = quatf::from_yaw_pitch_roll(aq).normalized();
    const auto m1 = catalyst::math::rotation_yaw_pitch_roll(am);

    const vec3f v{0.25f, -1.5f, 2.0f};

    const auto v0q = q0.rotate(v);
    const auto v1q = q1.rotate(v);
    CT_REQUIRE(catalyst::tests::nearly_equal(v0q.x, v1q.x));
    CT_REQUIRE(catalyst::tests::nearly_equal(v0q.y, v1q.y));
    CT_REQUIRE(catalyst::tests::nearly_equal(v0q.z, v1q.z));

    const auto v0m = m0 * v;
    const auto v1m = m1 * v;
    CT_REQUIRE(catalyst::tests::nearly_equal(v0m.x, v1m.x));
    CT_REQUIRE(catalyst::tests::nearly_equal(v0m.y, v1m.y));
    CT_REQUIRE(catalyst::tests::nearly_equal(v0m.z, v1m.z));
}

} // namespace

int main()
{
    test_euler_quat_vs_mat_rotation_match();
    test_euler_known_yaw_90_z();
    test_euler_from_degrees_helpers();
    test_euler_extraction_round_trip_matches_rotation();
    return 0;
}
