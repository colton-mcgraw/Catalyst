#include <catalyst/math/mat.hpp>

#include "test_common.hpp"

namespace
{

void test_mat_column_major_layout_and_access()
{
    using catalyst::math::mat;

    // Column-major initializer list:
    // col0 = [1,2], col1 = [3,4], col2 = [5,6]
    const mat<float, 2, 3> m{1, 2, 3, 4, 5, 6};

    CT_REQUIRE(catalyst::tests::nearly_equal(m(0, 0), 1.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(m(1, 0), 2.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(m(0, 1), 3.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(m(1, 1), 4.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(m(0, 2), 5.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(m(1, 2), 6.0f));

    const auto r0 = m.row(0);
    const auto r1 = m.row(1);
    CT_REQUIRE(catalyst::tests::nearly_equal(r0[0], 1.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(r0[1], 3.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(r0[2], 5.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(r1[0], 2.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(r1[1], 4.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(r1[2], 6.0f));
}

void test_mat_from_rows_matches_column_major()
{
    using catalyst::math::mat;
    using catalyst::math::vec;

    const auto m = mat<float, 2, 3>::from_rows({
        vec<float, 3>{1.0f, 3.0f, 5.0f},
        vec<float, 3>{2.0f, 4.0f, 6.0f},
    });

    CT_REQUIRE(catalyst::tests::nearly_equal(m(0, 0), 1.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(m(1, 0), 2.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(m(0, 1), 3.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(m(1, 1), 4.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(m(0, 2), 5.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(m(1, 2), 6.0f));
}

void test_mat_mul_vec_and_mul_mat()
{
    using catalyst::math::mat;
    using catalyst::math::vec;

    const mat<float, 2, 3> a{1, 2, 3, 4, 5, 6};
    const vec<float, 3> v{10.0f, 100.0f, 1000.0f};

    const auto r = a * v;
    CT_REQUIRE(catalyst::tests::nearly_equal(r[0], 5310.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(r[1], 6420.0f));

    // b is 3x2:
    // col0 = [1,3,5], col1 = [2,4,6]
    const mat<float, 3, 2> b{1, 3, 5, 2, 4, 6};
    const auto ab = a * b; // 2x2

    CT_REQUIRE(catalyst::tests::nearly_equal(ab(0, 0), 35.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(ab(1, 0), 44.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(ab(0, 1), 44.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(ab(1, 1), 56.0f));
}

void test_mat_identity()
{
    using catalyst::math::mat3f;
    using catalyst::math::vec3f;

    const auto I = mat3f::identity();
    const vec3f v{1.0f, 2.0f, 3.0f};
    const auto out = I * v;

    CT_REQUIRE(catalyst::tests::nearly_equal(out[0], v[0]));
    CT_REQUIRE(catalyst::tests::nearly_equal(out[1], v[1]));
    CT_REQUIRE(catalyst::tests::nearly_equal(out[2], v[2]));
}

} // namespace

int main()
{
    test_mat_column_major_layout_and_access();
    test_mat_from_rows_matches_column_major();
    test_mat_mul_vec_and_mul_mat();
    test_mat_identity();
    return 0;
}
