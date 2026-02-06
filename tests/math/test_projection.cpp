#include <catalyst/math/projection.hpp>

#include "test_common.hpp"

namespace
{

void test_perspective_rh_zo_near_far_map_to_0_1()
{
    using catalyst::math::radiansf;
    using catalyst::math::vec4f;

    const float pi = 3.14159265358979323846f;

    const float fov_y = pi * 0.5f;
    const float aspect = 16.0f / 9.0f;
    const float zn = 1.0f;
    const float zf = 10.0f;

    const auto p = catalyst::math::perspective_rh_zo(fov_y, aspect, zn, zf);
    const auto p_typed = catalyst::math::perspective_rh_zo(radiansf{fov_y}, aspect, zn, zf);

    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            CT_REQUIRE(catalyst::tests::nearly_equal(p(r, c), p_typed(r, c)));
        }
    }

    // In RH view space, forward is -Z.
    const vec4f v_near{0.0f, 0.0f, -zn, 1.0f};
    const vec4f v_far{0.0f, 0.0f, -zf, 1.0f};

    const auto c_near = p * v_near;
    const auto c_far = p * v_far;

    const float z_ndc_near = c_near.z / c_near.w;
    const float z_ndc_far = c_far.z / c_far.w;

    CT_REQUIRE(catalyst::tests::nearly_equal(z_ndc_near, 0.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(z_ndc_far, 1.0f));
}

void test_orthographic_lh_zo_near_far_map_to_0_1()
{
    using catalyst::math::vec4f;

    const float left = -2.0f;
    const float right = 2.0f;
    const float bottom = -1.0f;
    const float top = 1.0f;
    const float zn = 1.0f;
    const float zf = 10.0f;

    const auto o = catalyst::math::orthographic(
        left,
        right,
        bottom,
        top,
        zn,
        zf,
        catalyst::math::handedness::left,
        catalyst::math::depth_range::zero_to_one);

    // In LH view space, forward is +Z.
    const vec4f v_near{0.0f, 0.0f, zn, 1.0f};
    const vec4f v_far{0.0f, 0.0f, zf, 1.0f};

    const auto c_near = o * v_near;
    const auto c_far = o * v_far;

    const float z_ndc_near = c_near.z / c_near.w;
    const float z_ndc_far = c_far.z / c_far.w;

    CT_REQUIRE(catalyst::tests::nearly_equal(z_ndc_near, 0.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(z_ndc_far, 1.0f));
}

} // namespace

int main()
{
    test_perspective_rh_zo_near_far_map_to_0_1();
    test_orthographic_lh_zo_near_far_map_to_0_1();
    return 0;
}
