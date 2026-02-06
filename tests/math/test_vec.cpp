#include <catalyst/math/vec.hpp>

#include "test_common.hpp"

namespace
{

void test_vec2f_basics()
{
    using catalyst::math::vec2f;

    const vec2f a{1.0f, 2.0f};
    const vec2f b{3.0f, 4.0f};

    const auto c = a + b;
    CT_REQUIRE(catalyst::tests::nearly_equal(c.x, 4.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(c.y, 6.0f));

    const auto d = b - a;
    CT_REQUIRE(catalyst::tests::nearly_equal(d.x, 2.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(d.y, 2.0f));

    CT_REQUIRE(catalyst::tests::nearly_equal(a.dot(b), 11.0f));

    const auto n = vec2f{3.0f, 4.0f}.normalized();
    CT_REQUIRE(catalyst::tests::nearly_equal(n.length(), 1.0f));
}

void test_vecN_helpers()
{
    using catalyst::math::vec;

    const vec<float, 4> a{1.0f, 2.0f, 3.0f, 4.0f};
    const vec<float, 4> b{0.0f, 10.0f, 2.0f, 5.0f};

    const auto lo = a.min(b);
    CT_REQUIRE(catalyst::tests::nearly_equal(lo[0], 0.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(lo[1], 2.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(lo[2], 2.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(lo[3], 4.0f));

    const auto hi = a.max(b);
    CT_REQUIRE(catalyst::tests::nearly_equal(hi[0], 1.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(hi[1], 10.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(hi[2], 3.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(hi[3], 5.0f));

    const auto clamped = a.clamp(vec<float, 4>(1.5f), vec<float, 4>(3.5f));
    CT_REQUIRE(catalyst::tests::nearly_equal(clamped[0], 1.5f));
    CT_REQUIRE(catalyst::tests::nearly_equal(clamped[1], 2.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(clamped[2], 3.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(clamped[3], 3.5f));

    // Wider vectors (exercise chunked paths when available)
    const vec<float, 8> w0{1, 2, 3, 4, 5, 6, 7, 8};
    const vec<float, 8> w1{8, 7, 6, 5, 4, 3, 2, 1};
    const auto w2 = w0 + w1;
    CT_REQUIRE(catalyst::tests::nearly_equal(w2[0], 9.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(w2[7], 9.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(w0.dot(w1), 120.0f));

    const vec<double, 4> d0{1.0, 2.0, 3.0, 4.0};
    const vec<double, 4> d1{2.0, 3.0, 4.0, 5.0};
    CT_REQUIRE(catalyst::tests::nearly_equal(d0.dot(d1), 40.0));
}

void test_vec3_cross()
{
    using catalyst::math::vec3f;

    const vec3f ex{1.0f, 0.0f, 0.0f};
    const vec3f ey{0.0f, 1.0f, 0.0f};

    const auto ez = ex.cross(ey);
    CT_REQUIRE(catalyst::tests::nearly_equal(ez.x, 0.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(ez.y, 0.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(ez.z, 1.0f));
}

void test_vec_swizzles()
{
    using catalyst::math::vec2f;
    using catalyst::math::vec3f;
    using catalyst::math::vec4f;

    const vec2f a{1.0f, 2.0f};
    const auto ayx = a.yx();
    CT_REQUIRE(catalyst::tests::nearly_equal(ayx.x, 2.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(ayx.y, 1.0f));

    const vec3f b{1.0f, 2.0f, 3.0f};
    const auto bxy = b.xy();
    CT_REQUIRE(catalyst::tests::nearly_equal(bxy.x, 1.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(bxy.y, 2.0f));

    const auto bzyx = b.zyx();
    CT_REQUIRE(catalyst::tests::nearly_equal(bzyx.x, 3.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(bzyx.y, 2.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(bzyx.z, 1.0f));

    const vec4f c{1.0f, 2.0f, 3.0f, 4.0f};
    const auto czw = c.zw();
    CT_REQUIRE(catalyst::tests::nearly_equal(czw.x, 3.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(czw.y, 4.0f));

    const auto cwzyx = c.wzyx();
    CT_REQUIRE(catalyst::tests::nearly_equal(cwzyx.x, 4.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(cwzyx.y, 3.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(cwzyx.z, 2.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(cwzyx.w, 1.0f));

    const auto s = c.swizzle<3, 0, 0, 2>();
    CT_REQUIRE(catalyst::tests::nearly_equal(s.x, 4.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(s.y, 1.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(s.z, 1.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(s.w, 3.0f));
}

} // namespace

int main()
{
    test_vec2f_basics();
    test_vecN_helpers();
    test_vec3_cross();
    test_vec_swizzles();
    return 0;
}
