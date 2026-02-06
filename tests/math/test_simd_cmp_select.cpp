#include <catalyst/math/simd.hpp>

#include "test_common.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>

namespace
{

std::array<std::uint32_t, 4> lane_bits_mask32x4(catalyst::math::mask32x4 m)
{
    std::array<std::uint32_t, 4> bits{};
    m.store_unaligned(bits.data());
    return bits;
}

std::array<std::uint64_t, 2> lane_bits_mask64x2(catalyst::math::mask64x2 m)
{
    std::array<std::uint64_t, 2> bits{};
    m.store_unaligned(bits.data());
    return bits;
}

void test_f32x4_cmp_and_select()
{
    using catalyst::math::cmp_eq;
    using catalyst::math::cmp_ge;
    using catalyst::math::cmp_gt;
    using catalyst::math::cmp_le;
    using catalyst::math::cmp_lt;
    using catalyst::math::f32x4;
    using catalyst::math::select;
    using catalyst::math::all;
    using catalyst::math::any;

    const auto a = f32x4::set(1.0f, 2.0f, 3.0f, 4.0f);
    const auto b = f32x4::set(0.0f, 2.0f, 4.0f, 1.0f);

    {
        const auto m = cmp_eq(a, b);
        const auto bits = lane_bits_mask32x4(m);
        CT_REQUIRE(bits[0] == 0u);
        CT_REQUIRE(bits[1] == 0xFFFFFFFFu);
        CT_REQUIRE(bits[2] == 0u);
        CT_REQUIRE(bits[3] == 0u);

        CT_REQUIRE(any(m));
        CT_REQUIRE(!all(m));
    }

    {
        const auto m = cmp_lt(a, b);
        const auto bits = lane_bits_mask32x4(m);
        CT_REQUIRE(bits[0] == 0u);
        CT_REQUIRE(bits[1] == 0u);
        CT_REQUIRE(bits[2] == 0xFFFFFFFFu);
        CT_REQUIRE(bits[3] == 0u);

        CT_REQUIRE(any(m));
        CT_REQUIRE(!all(m));

        // select(mask, a, b) chooses `a` when mask is true, else `b`.
        float out[4]{};
        select(m, a, b).store_unaligned(out);
        CT_REQUIRE(catalyst::tests::nearly_equal(out[0], 0.0f));
        CT_REQUIRE(catalyst::tests::nearly_equal(out[1], 2.0f));
        CT_REQUIRE(catalyst::tests::nearly_equal(out[2], 3.0f));
        CT_REQUIRE(catalyst::tests::nearly_equal(out[3], 1.0f));
    }

    {
        const auto m = cmp_gt(a, b);
        const auto bits = lane_bits_mask32x4(m);
        CT_REQUIRE(bits[0] == 0xFFFFFFFFu);
        CT_REQUIRE(bits[1] == 0u);
        CT_REQUIRE(bits[2] == 0u);
        CT_REQUIRE(bits[3] == 0xFFFFFFFFu);

        CT_REQUIRE(any(m));
        CT_REQUIRE(!all(m));
    }

    {
        const auto m = cmp_le(a, b);
        const auto bits = lane_bits_mask32x4(m);
        CT_REQUIRE(bits[0] == 0u);
        CT_REQUIRE(bits[1] == 0xFFFFFFFFu);
        CT_REQUIRE(bits[2] == 0xFFFFFFFFu);
        CT_REQUIRE(bits[3] == 0u);

        CT_REQUIRE(any(m));
        CT_REQUIRE(!all(m));

        float out[4]{};
        select(m, a, b).store_unaligned(out);
        CT_REQUIRE(catalyst::tests::nearly_equal(out[0], 0.0f));
        CT_REQUIRE(catalyst::tests::nearly_equal(out[1], 2.0f));
        CT_REQUIRE(catalyst::tests::nearly_equal(out[2], 3.0f));
        CT_REQUIRE(catalyst::tests::nearly_equal(out[3], 1.0f));
    }

    {
        const auto m = cmp_ge(a, b);
        const auto bits = lane_bits_mask32x4(m);
        CT_REQUIRE(bits[0] == 0xFFFFFFFFu);
        CT_REQUIRE(bits[1] == 0xFFFFFFFFu);
        CT_REQUIRE(bits[2] == 0u);
        CT_REQUIRE(bits[3] == 0xFFFFFFFFu);

        CT_REQUIRE(any(m));
        CT_REQUIRE(!all(m));
    }

    {
        const auto m = cmp_eq(f32x4::splat(1.0f), f32x4::splat(1.0f));
        CT_REQUIRE(any(m));
        CT_REQUIRE(all(m));
    }

    // NaN comparisons should be false.
    {
        const float nan = (std::numeric_limits<float>::quiet_NaN)();
        const auto m = cmp_eq(f32x4::splat(nan), f32x4::splat(nan));
        const auto bits = lane_bits_mask32x4(m);
        CT_REQUIRE(bits[0] == 0u);
        CT_REQUIRE(bits[1] == 0u);
        CT_REQUIRE(bits[2] == 0u);
        CT_REQUIRE(bits[3] == 0u);

        CT_REQUIRE(!any(m));
        CT_REQUIRE(!all(m));
    }
}

void test_f64x2_cmp_and_select()
{
    using catalyst::math::cmp_eq;
    using catalyst::math::cmp_ge;
    using catalyst::math::cmp_gt;
    using catalyst::math::cmp_le;
    using catalyst::math::cmp_lt;
    using catalyst::math::f64x2;
    using catalyst::math::select;
    using catalyst::math::all;
    using catalyst::math::any;

    const auto a = f64x2::set(1.0, 2.0);
    const auto b = f64x2::set(0.0, 2.5);

    {
        const auto m = cmp_eq(a, b);
        const auto bits = lane_bits_mask64x2(m);
        CT_REQUIRE(bits[0] == 0ull);
        CT_REQUIRE(bits[1] == 0ull);

        CT_REQUIRE(!any(m));
        CT_REQUIRE(!all(m));
    }

    {
        const auto m = cmp_lt(a, b);
        const auto bits = lane_bits_mask64x2(m);
        CT_REQUIRE(bits[0] == 0ull);
        CT_REQUIRE(bits[1] == 0xFFFFFFFFFFFFFFFFull);

        CT_REQUIRE(any(m));
        CT_REQUIRE(!all(m));

        double out[2]{};
        select(m, a, b).store_unaligned(out);
        CT_REQUIRE(catalyst::tests::nearly_equal(out[0], 0.0));
        CT_REQUIRE(catalyst::tests::nearly_equal(out[1], 2.0));
    }

    {
        const auto m = cmp_gt(a, b);
        const auto bits = lane_bits_mask64x2(m);
        CT_REQUIRE(bits[0] == 0xFFFFFFFFFFFFFFFFull);
        CT_REQUIRE(bits[1] == 0ull);

        CT_REQUIRE(any(m));
        CT_REQUIRE(!all(m));
    }

    {
        const auto m = cmp_le(a, b);
        const auto bits = lane_bits_mask64x2(m);
        CT_REQUIRE(bits[0] == 0ull);
        CT_REQUIRE(bits[1] == 0xFFFFFFFFFFFFFFFFull);

        CT_REQUIRE(any(m));
        CT_REQUIRE(!all(m));
    }

    {
        const auto m = cmp_ge(a, b);
        const auto bits = lane_bits_mask64x2(m);
        CT_REQUIRE(bits[0] == 0xFFFFFFFFFFFFFFFFull);
        CT_REQUIRE(bits[1] == 0ull);

        CT_REQUIRE(any(m));
        CT_REQUIRE(!all(m));
    }

    {
        const auto m = cmp_eq(f64x2::splat(1.0), f64x2::splat(1.0));
        CT_REQUIRE(any(m));
        CT_REQUIRE(all(m));
    }

    // NaN comparisons should be false.
    {
        const double nan = (std::numeric_limits<double>::quiet_NaN)();
        const auto m = cmp_eq(f64x2::splat(nan), f64x2::splat(nan));
        const auto bits = lane_bits_mask64x2(m);
        CT_REQUIRE(bits[0] == 0ull);
        CT_REQUIRE(bits[1] == 0ull);

        CT_REQUIRE(!any(m));
        CT_REQUIRE(!all(m));
    }
}

} // namespace

int main()
{
    test_f32x4_cmp_and_select();
    test_f64x2_cmp_and_select();
    return 0;
}
