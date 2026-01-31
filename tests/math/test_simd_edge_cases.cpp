#include <catalyst/math/simd.hpp>

#include "test_common.hpp"

#include <limits>

namespace
{
    using catalyst::tests::lane_bits;
    using catalyst::tests::lanes;
    using catalyst::tests::nearly_equal;

    void test_constructors_zero_splat_set()
    {
        using catalyst::math::f32x4;

        const auto z = lanes(f32x4::zero());
        CT_REQUIRE(z[0] == 0.0f);
        CT_REQUIRE(z[1] == 0.0f);
        CT_REQUIRE(z[2] == 0.0f);
        CT_REQUIRE(z[3] == 0.0f);

        const auto s = lanes(f32x4::splat(3.25f));
        CT_REQUIRE(nearly_equal(s[0], 3.25f));
        CT_REQUIRE(nearly_equal(s[1], 3.25f));
        CT_REQUIRE(nearly_equal(s[2], 3.25f));
        CT_REQUIRE(nearly_equal(s[3], 3.25f));

        const auto v = lanes(f32x4::set(1.0f, 2.0f, 3.0f, 4.0f));
        CT_REQUIRE(nearly_equal(v[0], 1.0f));
        CT_REQUIRE(nearly_equal(v[1], 2.0f));
        CT_REQUIRE(nearly_equal(v[2], 3.0f));
        CT_REQUIRE(nearly_equal(v[3], 4.0f));
    }

    void test_basic_operators()
    {
        using catalyst::math::f32x4;

        const f32x4 a = f32x4::set(1.0f, -2.0f, 3.0f, 4.0f);
        const f32x4 b = f32x4::set(2.0f, 5.0f, -1.0f, 0.5f);

        const auto add = lanes(a + b);
        CT_REQUIRE(nearly_equal(add[0], 3.0f));
        CT_REQUIRE(nearly_equal(add[1], 3.0f));
        CT_REQUIRE(nearly_equal(add[2], 2.0f));
        CT_REQUIRE(nearly_equal(add[3], 4.5f));

        const auto sub = lanes(a - b);
        CT_REQUIRE(nearly_equal(sub[0], -1.0f));
        CT_REQUIRE(nearly_equal(sub[1], -7.0f));
        CT_REQUIRE(nearly_equal(sub[2], 4.0f));
        CT_REQUIRE(nearly_equal(sub[3], 3.5f));

        const auto mul = lanes(a * b);
        CT_REQUIRE(nearly_equal(mul[0], 2.0f));
        CT_REQUIRE(nearly_equal(mul[1], -10.0f));
        CT_REQUIRE(nearly_equal(mul[2], -3.0f));
        CT_REQUIRE(nearly_equal(mul[3], 2.0f));

        const auto div = lanes(a / b);
        CT_REQUIRE(nearly_equal(div[0], 0.5f));
        CT_REQUIRE(nearly_equal(div[1], -0.4f));
        CT_REQUIRE(nearly_equal(div[2], -3.0f));
        CT_REQUIRE(nearly_equal(div[3], 8.0f));
    }

    void test_mask_edge_cases()
    {
        using catalyst::math::f32x4;

        const float neg_zero = -0.0f;
        const auto v = f32x4::set(neg_zero, 0.0f, -1.0f, 1.0f);

        // mask() is implemented as (x < 0), not sign-bit.
        const auto mb = lane_bits(v.mask());
        CT_REQUIRE(mb[0] == 0x00000000u);
        CT_REQUIRE(mb[1] == 0x00000000u);
        CT_REQUIRE(mb[2] == 0xFFFFFFFFu);
        CT_REQUIRE(mb[3] == 0x00000000u);

        const float nan = std::numeric_limits<float>::quiet_NaN();
        const auto n = f32x4::set(nan, nan, nan, nan);
        const auto mn = lane_bits(n.mask());
        CT_REQUIRE(mn[0] == 0x00000000u);
        CT_REQUIRE(mn[1] == 0x00000000u);
        CT_REQUIRE(mn[2] == 0x00000000u);
        CT_REQUIRE(mn[3] == 0x00000000u);
    }

    void test_nan_inf_behavior()
    {
        using catalyst::math::f32x4;

        const float nan = std::numeric_limits<float>::quiet_NaN();

        const auto a = f32x4::set(nan, -1.0f, 0.0f, 2.0f);
        const auto ab = lanes(a.abs());
        CT_REQUIRE(std::isnan(ab[0]));
        CT_REQUIRE(nearly_equal(ab[1], 1.0f));
        CT_REQUIRE(nearly_equal(ab[2], 0.0f));
        CT_REQUIRE(nearly_equal(ab[3], 2.0f));

        // sqrt(negative) should produce NaN for that lane.
        const auto s = lanes(f32x4::set(-1.0f, 4.0f, 0.0f, 9.0f).sqrt());
        CT_REQUIRE(std::isnan(s[0]));
        CT_REQUIRE(nearly_equal(s[1], 2.0f));
        CT_REQUIRE(nearly_equal(s[2], 0.0f));
        CT_REQUIRE(nearly_equal(s[3], 3.0f));

        // reciprocal(+0) and rsqrt(+0) should be +inf in IEEE floats.
        const auto r0 = lanes(f32x4::set(0.0f, 2.0f, -4.0f, 0.5f).reciprocal());
        CT_REQUIRE(std::isinf(r0[0]));
        CT_REQUIRE(r0[0] > 0.0f);
        CT_REQUIRE(nearly_equal(r0[1], 0.5f));
        CT_REQUIRE(nearly_equal(r0[2], -0.25f));
        CT_REQUIRE(nearly_equal(r0[3], 2.0f));

        const auto rs0 = lanes(f32x4::set(0.0f, 4.0f, 9.0f, 16.0f).rsqrt());
        CT_REQUIRE(std::isinf(rs0[0]));
        CT_REQUIRE(rs0[0] > 0.0f);
        CT_REQUIRE(nearly_equal(rs0[1], 0.5f));
        CT_REQUIRE(nearly_equal(rs0[2], 1.0f / 3.0f));
        CT_REQUIRE(nearly_equal(rs0[3], 0.25f));
    }

} // namespace

int main()
{
    test_constructors_zero_splat_set();
    test_basic_operators();
    test_mask_edge_cases();
    test_nan_inf_behavior();

    std::cout << "All SIMD edge-case tests passed.\n";
    return 0;
}
