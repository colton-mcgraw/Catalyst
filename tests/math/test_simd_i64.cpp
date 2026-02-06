#include <catalyst/math/simd.hpp>

#include "test_common.hpp"

#include <cstdint>

namespace
{

void test_i64x2_load_store_and_ops()
{
    using catalyst::math::i64x2;

    alignas(16) std::int64_t in[2] = {123, -456};
    const auto a = i64x2::load_aligned(in);

    std::int64_t out[2]{};
    a.store_unaligned(out);
    CT_REQUIRE(out[0] == 123);
    CT_REQUIRE(out[1] == -456);

    const auto b = i64x2::set(7, 9);

    (a + b).store_unaligned(out);
    CT_REQUIRE(out[0] == 130);
    CT_REQUIRE(out[1] == -447);

    (a - b).store_unaligned(out);
    CT_REQUIRE(out[0] == 116);
    CT_REQUIRE(out[1] == -465);

    const auto c = i64x2::set(0x0F0F0F0F0F0F0F0FLL, 0x00FF00FF00FF00FFLL);
    const auto d = i64x2::set(0x3333333333333333LL, 0x0F0F0F0F0F0F0F0FLL);

    (c & d).store_unaligned(out);
    CT_REQUIRE(out[0] == (0x0F0F0F0F0F0F0F0FLL & 0x3333333333333333LL));
    CT_REQUIRE(out[1] == (0x00FF00FF00FF00FFLL & 0x0F0F0F0F0F0F0F0FLL));

    (c | d).store_unaligned(out);
    CT_REQUIRE(out[0] == (0x0F0F0F0F0F0F0F0FLL | 0x3333333333333333LL));
    CT_REQUIRE(out[1] == (0x00FF00FF00FF00FFLL | 0x0F0F0F0F0F0F0F0FLL));

    (c ^ d).store_unaligned(out);
    CT_REQUIRE(out[0] == (0x0F0F0F0F0F0F0F0FLL ^ 0x3333333333333333LL));
    CT_REQUIRE(out[1] == (0x00FF00FF00FF00FFLL ^ 0x0F0F0F0F0F0F0F0FLL));

    (~c).store_unaligned(out);
    CT_REQUIRE(out[0] == (~0x0F0F0F0F0F0F0F0FLL));
    CT_REQUIRE(out[1] == (~0x00FF00FF00FF00FFLL));

    auto e = i64x2::set(1, 2);
    e += i64x2::splat(10);
    e.store_unaligned(out);
    CT_REQUIRE(out[0] == 11);
    CT_REQUIRE(out[1] == 12);

    e -= i64x2::set(1, 2);
    e.store_unaligned(out);
    CT_REQUIRE(out[0] == 10);
    CT_REQUIRE(out[1] == 10);

    e &= i64x2::set(0xFF, 0x0F);
    e.store_unaligned(out);
    CT_REQUIRE(out[0] == (10 & 0xFF));
    CT_REQUIRE(out[1] == (10 & 0x0F));
}

void test_u64x2_load_store_and_ops()
{
    using catalyst::math::u64x2;

    alignas(16) std::uint64_t in[2] = {123u, 456u};
    const auto a = u64x2::load_aligned(in);

    std::uint64_t out[2]{};
    a.store_unaligned(out);
    CT_REQUIRE(out[0] == 123u);
    CT_REQUIRE(out[1] == 456u);

    const auto b = u64x2::set(7u, 9u);

    (a + b).store_unaligned(out);
    CT_REQUIRE(out[0] == 130u);
    CT_REQUIRE(out[1] == 465u);

    (a - b).store_unaligned(out);
    CT_REQUIRE(out[0] == 116u);
    CT_REQUIRE(out[1] == 447u);

    const auto c = u64x2::set(0x0F0F0F0F0F0F0F0FULL, 0x00FF00FF00FF00FFULL);
    const auto d = u64x2::set(0x3333333333333333ULL, 0x0F0F0F0F0F0F0F0FULL);

    (c & d).store_unaligned(out);
    CT_REQUIRE(out[0] == (0x0F0F0F0F0F0F0F0FULL & 0x3333333333333333ULL));
    CT_REQUIRE(out[1] == (0x00FF00FF00FF00FFULL & 0x0F0F0F0F0F0F0F0FULL));

    (c | d).store_unaligned(out);
    CT_REQUIRE(out[0] == (0x0F0F0F0F0F0F0F0FULL | 0x3333333333333333ULL));
    CT_REQUIRE(out[1] == (0x00FF00FF00FF00FFULL | 0x0F0F0F0F0F0F0F0FULL));

    (c ^ d).store_unaligned(out);
    CT_REQUIRE(out[0] == (0x0F0F0F0F0F0F0F0FULL ^ 0x3333333333333333ULL));
    CT_REQUIRE(out[1] == (0x00FF00FF00FF00FFULL ^ 0x0F0F0F0F0F0F0F0FULL));

    (~c).store_unaligned(out);
    CT_REQUIRE(out[0] == (~0x0F0F0F0F0F0F0F0FULL));
    CT_REQUIRE(out[1] == (~0x00FF00FF00FF00FFULL));

    auto e = u64x2::set(1u, 2u);
    e += u64x2::splat(10u);
    e.store_unaligned(out);
    CT_REQUIRE(out[0] == 11u);
    CT_REQUIRE(out[1] == 12u);

    e -= u64x2::set(1u, 2u);
    e.store_unaligned(out);
    CT_REQUIRE(out[0] == 10u);
    CT_REQUIRE(out[1] == 10u);

    e &= u64x2::set(0xFFu, 0x0Fu);
    e.store_unaligned(out);
    CT_REQUIRE(out[0] == (10u & 0xFFu));
    CT_REQUIRE(out[1] == (10u & 0x0Fu));
}

} // namespace

int main()
{
    test_i64x2_load_store_and_ops();
    test_u64x2_load_store_and_ops();
    return 0;
}
