#include <catalyst/math/simd.hpp>

#include "test_common.hpp"

#include <cmath>
#include <limits>

namespace
{

void test_f64x2_load_store_ops()
{
    using catalyst::math::f64x2;

    alignas(16) double in[2] = {1.5, -2.25};
    const auto a = f64x2::load_aligned(in);

    double out[2]{};
    a.store_unaligned(out);
    CT_REQUIRE(catalyst::tests::nearly_equal(out[0], 1.5));
    CT_REQUIRE(catalyst::tests::nearly_equal(out[1], -2.25));

    const auto b = f64x2::set(2.0, 4.0);

    (a + b).store_unaligned(out);
    CT_REQUIRE(catalyst::tests::nearly_equal(out[0], 3.5));
    CT_REQUIRE(catalyst::tests::nearly_equal(out[1], 1.75));

    (b - a).store_unaligned(out);
    CT_REQUIRE(catalyst::tests::nearly_equal(out[0], 0.5));
    CT_REQUIRE(catalyst::tests::nearly_equal(out[1], 6.25));

    (a * b).store_unaligned(out);
    CT_REQUIRE(catalyst::tests::nearly_equal(out[0], 3.0));
    CT_REQUIRE(catalyst::tests::nearly_equal(out[1], -9.0));

    (b / f64x2::set(2.0, 8.0)).store_unaligned(out);
    CT_REQUIRE(catalyst::tests::nearly_equal(out[0], 1.0));
    CT_REQUIRE(catalyst::tests::nearly_equal(out[1], 0.5));
}

void test_f64x2_math_helpers()
{
    using catalyst::math::f64x2;

    const auto a = f64x2::set(-3.0, 4.0);
    double out[2]{};

    a.abs().store_unaligned(out);
    CT_REQUIRE(catalyst::tests::nearly_equal(out[0], 3.0));
    CT_REQUIRE(catalyst::tests::nearly_equal(out[1], 4.0));

    const auto c = f64x2::set(4.0, 9.0);
    c.sqrt().store_unaligned(out);
    CT_REQUIRE(catalyst::tests::nearly_equal(out[0], 2.0));
    CT_REQUIRE(catalyst::tests::nearly_equal(out[1], 3.0));

    c.reciprocal().store_unaligned(out);
    CT_REQUIRE(catalyst::tests::nearly_equal(out[0], 0.25));
    CT_REQUIRE(catalyst::tests::nearly_equal(out[1], 1.0 / 9.0));

    c.rsqrt().store_unaligned(out);
    CT_REQUIRE(catalyst::tests::nearly_equal(out[0], 0.5));
    CT_REQUIRE(catalyst::tests::nearly_equal(out[1], 1.0 / 3.0));

    const auto d = f64x2::set(1.0, 100.0);
    d.min(f64x2::set(2.0, -5.0)).store_unaligned(out);
    CT_REQUIRE(catalyst::tests::nearly_equal(out[0], 1.0));
    CT_REQUIRE(catalyst::tests::nearly_equal(out[1], -5.0));

    d.max(f64x2::set(2.0, -5.0)).store_unaligned(out);
    CT_REQUIRE(catalyst::tests::nearly_equal(out[0], 2.0));
    CT_REQUIRE(catalyst::tests::nearly_equal(out[1], 100.0));

    // sqrt(negative) -> NaN lane
    const auto s = f64x2::set(-1.0, 4.0).sqrt();
    s.store_unaligned(out);
    CT_REQUIRE(std::isnan(out[0]));
    CT_REQUIRE(catalyst::tests::nearly_equal(out[1], 2.0));

    // reciprocal(0) -> +inf lane
    const auto r0 = f64x2::set(0.0, 2.0).reciprocal();
    r0.store_unaligned(out);
    CT_REQUIRE(std::isinf(out[0]));
    CT_REQUIRE(out[0] > 0.0);
    CT_REQUIRE(catalyst::tests::nearly_equal(out[1], 0.5));
}

} // namespace

int main()
{
    test_f64x2_load_store_ops();
    test_f64x2_math_helpers();
    return 0;
}
