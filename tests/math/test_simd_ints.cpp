#include <catalyst/math/simd.hpp>

#include "test_common.hpp"

namespace
{

void test_i32x4_add_sub()
{
    using catalyst::math::i32x4;

    const i32x4 a = i32x4::set(1, -2, 3, 4);
    const i32x4 b = i32x4::set(5, 6, -7, 8);

    std::int32_t out[4]{};
    (a + b).store_unaligned(out);
    CT_REQUIRE(out[0] == 6);
    CT_REQUIRE(out[1] == 4);
    CT_REQUIRE(out[2] == -4);
    CT_REQUIRE(out[3] == 12);

    (a - b).store_unaligned(out);
    CT_REQUIRE(out[0] == -4);
    CT_REQUIRE(out[1] == -8);
    CT_REQUIRE(out[2] == 10);
    CT_REQUIRE(out[3] == -4);
}

void test_i32x4_bitwise()
{
    using catalyst::math::i32x4;

    const i32x4 a = i32x4::set(0x0F0F0F0F, 0xAAAAAAAA, 0x00000000, 0xFFFFFFFF);
    const i32x4 b = i32x4::set(0x00FF00FF, 0x55555555, 0xFFFFFFFF, 0x00000000);

    std::int32_t out[4]{};

    (a & b).store_unaligned(out);
    CT_REQUIRE(static_cast<std::uint32_t>(out[0]) == 0x000F000Fu);
    CT_REQUIRE(static_cast<std::uint32_t>(out[1]) == 0x00000000u);
    CT_REQUIRE(static_cast<std::uint32_t>(out[2]) == 0x00000000u);
    CT_REQUIRE(static_cast<std::uint32_t>(out[3]) == 0x00000000u);

    (a | b).store_unaligned(out);
    CT_REQUIRE(static_cast<std::uint32_t>(out[0]) == 0x0FFF0FFFu);
    CT_REQUIRE(static_cast<std::uint32_t>(out[1]) == 0xFFFFFFFFu);
    CT_REQUIRE(static_cast<std::uint32_t>(out[2]) == 0xFFFFFFFFu);
    CT_REQUIRE(static_cast<std::uint32_t>(out[3]) == 0xFFFFFFFFu);

    (a ^ b).store_unaligned(out);
    CT_REQUIRE(static_cast<std::uint32_t>(out[0]) == 0x0FF00FF0u);
    CT_REQUIRE(static_cast<std::uint32_t>(out[1]) == 0xFFFFFFFFu);
    CT_REQUIRE(static_cast<std::uint32_t>(out[2]) == 0xFFFFFFFFu);
    CT_REQUIRE(static_cast<std::uint32_t>(out[3]) == 0xFFFFFFFFu);

    (~a).store_unaligned(out);
    CT_REQUIRE(static_cast<std::uint32_t>(out[0]) == 0xF0F0F0F0u);
    CT_REQUIRE(static_cast<std::uint32_t>(out[1]) == 0x55555555u);
    CT_REQUIRE(static_cast<std::uint32_t>(out[2]) == 0xFFFFFFFFu);
    CT_REQUIRE(static_cast<std::uint32_t>(out[3]) == 0x00000000u);
}

void test_u32x4_add_sub_and_bitwise()
{
    using catalyst::math::u32x4;

    const u32x4 a = u32x4::set(1u, 2u, 3u, 4u);
    const u32x4 b = u32x4::set(10u, 20u, 30u, 40u);

    std::uint32_t out[4]{};
    (a + b).store_unaligned(out);
    CT_REQUIRE(out[0] == 11u);
    CT_REQUIRE(out[1] == 22u);
    CT_REQUIRE(out[2] == 33u);
    CT_REQUIRE(out[3] == 44u);

    (b - a).store_unaligned(out);
    CT_REQUIRE(out[0] == 9u);
    CT_REQUIRE(out[1] == 18u);
    CT_REQUIRE(out[2] == 27u);
    CT_REQUIRE(out[3] == 36u);

    const u32x4 m = u32x4::set(0xFF00FF00u, 0x0u, 0xFFFFFFFFu, 0x12345678u);
    const u32x4 n = u32x4::set(0x00FF00FFu, 0xFFFFFFFFu, 0x0u, 0xFFFF0000u);

    (m & n).store_unaligned(out);
    CT_REQUIRE(out[0] == 0x00000000u);
    CT_REQUIRE(out[1] == 0x00000000u);
    CT_REQUIRE(out[2] == 0x00000000u);
    CT_REQUIRE(out[3] == 0x12340000u);

    (~m).store_unaligned(out);
    CT_REQUIRE(out[0] == 0x00FF00FFu);
}

} // namespace

int main()
{
    test_i32x4_add_sub();
    test_i32x4_bitwise();
    test_u32x4_add_sub_and_bitwise();
    return 0;
}
