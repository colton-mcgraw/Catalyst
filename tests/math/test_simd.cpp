#include <catalyst/math/simd.hpp>

#include "test_common.hpp"

namespace
{
using catalyst::tests::lane_bits;
using catalyst::tests::lanes;
using catalyst::tests::nearly_equal;

void test_load_store_roundtrip()
{
    alignas(16) float in[4] = {1.25f, -2.5f, 3.0f, 100.0f};

    const auto a = catalyst::math::f32x4::load_aligned(in);
    auto out = lanes(a);

    CT_REQUIRE(nearly_equal(out[0], 1.25f));
    CT_REQUIRE(nearly_equal(out[1], -2.5f));
    CT_REQUIRE(nearly_equal(out[2], 3.0f));
    CT_REQUIRE(nearly_equal(out[3], 100.0f));

    const float in2[4] = {-1.0f, 2.0f, -3.5f, 4.25f};
    const auto b = catalyst::math::f32x4::load_unaligned(in2);
    out = lanes(b);

    CT_REQUIRE(nearly_equal(out[0], -1.0f));
    CT_REQUIRE(nearly_equal(out[1], 2.0f));
    CT_REQUIRE(nearly_equal(out[2], -3.5f));
    CT_REQUIRE(nearly_equal(out[3], 4.25f));
}

void test_abs_and_mask()
{
    const auto a = catalyst::math::f32x4::set(-1.0f, 2.0f, -3.5f, 4.25f);

    const auto ab = lanes(a.abs());
    CT_REQUIRE(nearly_equal(ab[0], 1.0f));
    CT_REQUIRE(nearly_equal(ab[1], 2.0f));
    CT_REQUIRE(nearly_equal(ab[2], 3.5f));
    CT_REQUIRE(nearly_equal(ab[3], 4.25f));

    // mask(): 0xFFFFFFFF for negative lanes, else 0.
    const auto mb = lane_bits(a.mask());
    CT_REQUIRE(mb[0] == 0xFFFFFFFFu);
    CT_REQUIRE(mb[1] == 0x00000000u);
    CT_REQUIRE(mb[2] == 0xFFFFFFFFu);
    CT_REQUIRE(mb[3] == 0x00000000u);
}

void test_min_max_elementwise_and_horizontal()
{
    const auto a = catalyst::math::f32x4::set(-1.0f, 2.0f, -3.5f, 4.25f);
    const auto b = catalyst::math::f32x4::set(0.5f, -10.0f, 3.0f, -2.0f);

    const auto mn = lanes(a.min(b));
    CT_REQUIRE(nearly_equal(mn[0], -1.0f));
    CT_REQUIRE(nearly_equal(mn[1], -10.0f));
    CT_REQUIRE(nearly_equal(mn[2], -3.5f));
    CT_REQUIRE(nearly_equal(mn[3], -2.0f));

    const auto mx = lanes(a.max(b));
    CT_REQUIRE(nearly_equal(mx[0], 0.5f));
    CT_REQUIRE(nearly_equal(mx[1], 2.0f));
    CT_REQUIRE(nearly_equal(mx[2], 3.0f));
    CT_REQUIRE(nearly_equal(mx[3], 4.25f));

    const auto hmn = lanes(a.min());
    CT_REQUIRE(nearly_equal(hmn[0], -3.5f));
    CT_REQUIRE(nearly_equal(hmn[1], -3.5f));
    CT_REQUIRE(nearly_equal(hmn[2], -3.5f));
    CT_REQUIRE(nearly_equal(hmn[3], -3.5f));

    const auto hmx = lanes(a.max());
    CT_REQUIRE(nearly_equal(hmx[0], 4.25f));
    CT_REQUIRE(nearly_equal(hmx[1], 4.25f));
    CT_REQUIRE(nearly_equal(hmx[2], 4.25f));
    CT_REQUIRE(nearly_equal(hmx[3], 4.25f));
}

void test_sqrt_reciprocal_rsqrt()
{
    const auto c = catalyst::math::f32x4::set(4.0f, 9.0f, 16.0f, 25.0f);

    const auto s = lanes(c.sqrt());
    CT_REQUIRE(nearly_equal(s[0], 2.0f));
    CT_REQUIRE(nearly_equal(s[1], 3.0f));
    CT_REQUIRE(nearly_equal(s[2], 4.0f));
    CT_REQUIRE(nearly_equal(s[3], 5.0f));

    const auto r = lanes(c.reciprocal());
    CT_REQUIRE(nearly_equal(r[0], 0.25f));
    CT_REQUIRE(nearly_equal(r[1], 1.0f / 9.0f));
    CT_REQUIRE(nearly_equal(r[2], 0.0625f));
    CT_REQUIRE(nearly_equal(r[3], 0.04f));

    const auto rs = lanes(c.rsqrt());
    CT_REQUIRE(nearly_equal(rs[0], 0.5f));
    CT_REQUIRE(nearly_equal(rs[1], 1.0f / 3.0f));
    CT_REQUIRE(nearly_equal(rs[2], 0.25f));
    CT_REQUIRE(nearly_equal(rs[3], 0.2f));
}

} // namespace

int main()
{
    test_load_store_roundtrip();
    test_abs_and_mask();
    test_min_max_elementwise_and_horizontal();
    test_sqrt_reciprocal_rsqrt();

    std::cout << "All SIMD functional tests passed.\n";
    return 0;
}
