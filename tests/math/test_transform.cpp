#include <catalyst/math/transform.hpp>

#include "test_common.hpp"

namespace
{

void test_translation_affects_points_not_vectors()
{
    using catalyst::math::mat4f;
    using catalyst::math::vec3f;

    const vec3f t{1.0f, 2.0f, 3.0f};
    const mat4f m = catalyst::math::translation(t);

    const vec3f p{4.0f, 5.0f, 6.0f};
    const vec3f v{4.0f, 5.0f, 6.0f};

    const auto p_out = catalyst::math::transform_point(m, p);
    const auto v_out = catalyst::math::transform_vector(m, v);

    CT_REQUIRE(catalyst::tests::nearly_equal(p_out.x, 5.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(p_out.y, 7.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(p_out.z, 9.0f));

    CT_REQUIRE(catalyst::tests::nearly_equal(v_out.x, v.x));
    CT_REQUIRE(catalyst::tests::nearly_equal(v_out.y, v.y));
    CT_REQUIRE(catalyst::tests::nearly_equal(v_out.z, v.z));
}

void test_trs_matches_manual_application()
{
    using catalyst::math::quatf;
    using catalyst::math::vec3f;

    const float pi = 3.14159265358979323846f;

    const vec3f t{1.0f, 2.0f, 3.0f};
    const vec3f s{2.0f, 3.0f, 4.0f};
    const quatf r = quatf::from_axis_angle(vec3f{0.0f, 0.0f, 1.0f}, pi * 0.5f).normalized();

    const auto m = catalyst::math::trs(t, r, s);

    const vec3f v{1.0f, 0.0f, 0.0f};

    const auto mv = catalyst::math::transform_point(m, v);

    // Manual: v' = t + r.rotate(s * v)
    const vec3f sv{s.x * v.x, s.y * v.y, s.z * v.z};
    const vec3f rv = r.rotate(sv);
    const vec3f expected{t.x + rv.x, t.y + rv.y, t.z + rv.z};

    CT_REQUIRE(catalyst::tests::nearly_equal(mv.x, expected.x));
    CT_REQUIRE(catalyst::tests::nearly_equal(mv.y, expected.y));
    CT_REQUIRE(catalyst::tests::nearly_equal(mv.z, expected.z));
}

} // namespace

int main()
{
    test_translation_affects_points_not_vectors();
    test_trs_matches_manual_application();
    return 0;
}
