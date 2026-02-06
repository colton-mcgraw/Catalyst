#include <catalyst/math/quat.hpp>

#include "test_common.hpp"

namespace
{

void test_quat_identity_rotation()
{
    using catalyst::math::quatf;
    using catalyst::math::vec3f;

    const quatf q = quatf::identity();
    const vec3f v{1.0f, 2.0f, 3.0f};
    const auto out = q.rotate(v);

    CT_REQUIRE(catalyst::tests::nearly_equal(out.x, v.x));
    CT_REQUIRE(catalyst::tests::nearly_equal(out.y, v.y));
    CT_REQUIRE(catalyst::tests::nearly_equal(out.z, v.z));
}

void test_quat_axis_angle_rotate_90_deg_z()
{
    using catalyst::math::quatf;
    using catalyst::math::degreesf;
    using catalyst::math::radiansf;
    using catalyst::math::vec3f;

    const float pi = 3.14159265358979323846f;

    const float r = pi * 0.5f;
    const quatf q = quatf::from_axis_angle(vec3f{0.0f, 0.0f, 1.0f}, radiansf{r});
    const vec3f v{1.0f, 0.0f, 0.0f};
    const auto out = q.rotate(v);

    CT_REQUIRE(catalyst::tests::nearly_equal(out.x, 0.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(out.y, 1.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(out.z, 0.0f));

    const quatf q_deg = quatf::from_axis_angle_degrees(vec3f{0.0f, 0.0f, 1.0f}, degreesf{90.0f});
    const auto out_deg = q_deg.rotate(v);
    CT_REQUIRE(catalyst::tests::nearly_equal(out_deg.x, 0.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(out_deg.y, 1.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(out_deg.z, 0.0f));
}

void test_quat_yaw_pitch_roll_degrees_angle_types()
{
    using catalyst::math::quatf;
    using catalyst::math::degreesf;
    using catalyst::math::vec3f;

    const quatf q_a = quatf::from_yaw_pitch_roll_degrees(degreesf{90.0f}, degreesf{0.0f}, degreesf{0.0f});
    const quatf q_b = quatf::from_yaw_pitch_roll_degrees(90.0f, 0.0f, 0.0f);

    const vec3f v{1.0f, 0.0f, 0.0f};
    const auto out_a = q_a.rotate(v);
    const auto out_b = q_b.rotate(v);

    CT_REQUIRE(catalyst::tests::nearly_equal(out_a.x, out_b.x));
    CT_REQUIRE(catalyst::tests::nearly_equal(out_a.y, out_b.y));
    CT_REQUIRE(catalyst::tests::nearly_equal(out_a.z, out_b.z));
}

void test_quat_mul_composes_rotations()
{
    using catalyst::math::quatf;
    using catalyst::math::vec3f;

    const float pi = 3.14159265358979323846f;

    const quatf qz = quatf::from_axis_angle(vec3f{0.0f, 0.0f, 1.0f}, pi * 0.5f);
    const quatf qy = quatf::from_axis_angle(vec3f{0.0f, 1.0f, 0.0f}, pi * 0.5f);

    // Apply y then z
    const quatf q = (qz * qy).normalized();

    const vec3f v{1.0f, 0.0f, 0.0f};
    const auto out_step = qz.rotate(qy.rotate(v));
    const auto out_one = q.rotate(v);

    CT_REQUIRE(catalyst::tests::nearly_equal(out_one.x, out_step.x));
    CT_REQUIRE(catalyst::tests::nearly_equal(out_one.y, out_step.y));
    CT_REQUIRE(catalyst::tests::nearly_equal(out_one.z, out_step.z));
}

void test_quat_inverse_undoes_rotation()
{
    using catalyst::math::quatf;
    using catalyst::math::vec3f;

    const float pi = 3.14159265358979323846f;

    const quatf q = quatf::from_axis_angle(vec3f{1.0f, 2.0f, 3.0f}, pi * 0.75f).normalized();
    const quatf inv = q.inverse();

    const vec3f v{4.0f, -2.0f, 1.0f};
    const auto out = inv.rotate(q.rotate(v));

    CT_REQUIRE(catalyst::tests::nearly_equal(out.x, v.x));
    CT_REQUIRE(catalyst::tests::nearly_equal(out.y, v.y));
    CT_REQUIRE(catalyst::tests::nearly_equal(out.z, v.z));
}

void test_quat_to_mat3_matches_rotation()
{
    using catalyst::math::quatf;
    using catalyst::math::vec3f;

    const float pi = 3.14159265358979323846f;

    const quatf q = quatf::from_axis_angle(vec3f{0.0f, 0.0f, 1.0f}, pi * 0.5f);
    const auto m = q.to_mat3();

    const vec3f v{1.0f, 0.0f, 0.0f};
    const auto out_q = q.rotate(v);
    const auto out_m = m * v;

    CT_REQUIRE(catalyst::tests::nearly_equal(out_m.x, out_q.x));
    CT_REQUIRE(catalyst::tests::nearly_equal(out_m.y, out_q.y));
    CT_REQUIRE(catalyst::tests::nearly_equal(out_m.z, out_q.z));
}

} // namespace

int main()
{
    test_quat_identity_rotation();
    test_quat_axis_angle_rotate_90_deg_z();
    test_quat_yaw_pitch_roll_degrees_angle_types();
    test_quat_mul_composes_rotations();
    test_quat_inverse_undoes_rotation();
    test_quat_to_mat3_matches_rotation();
    return 0;
}
