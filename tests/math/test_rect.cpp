#include <catalyst/math/rect.hpp>

#include "test_common.hpp"

namespace
{

void test_rect_contains_and_intersection()
{
    using catalyst::math::rectf;
    using catalyst::math::vec2f;

    const rectf a = rectf::from_xywh(0.0f, 0.0f, 10.0f, 10.0f);
    const rectf b = rectf::from_xywh(5.0f, 5.0f, 10.0f, 10.0f);

    CT_REQUIRE(a.contains(vec2f{0.0f, 0.0f}));
    CT_REQUIRE(!a.contains(vec2f{10.0f, 10.0f}));
    CT_REQUIRE(!a.contains(vec2f{-1.0f, 0.0f}));

    CT_REQUIRE(a.intersects(b));

    const auto i = a.intersection(b);
    CT_REQUIRE(catalyst::tests::nearly_equal(i.min.x, 5.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(i.min.y, 5.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(i.max.x, 10.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(i.max.y, 10.0f));
}

void test_rect_unite_and_clamp()
{
    using catalyst::math::rectf;
    using catalyst::math::vec2f;

    const rectf a = rectf::from_xywh(0.0f, 0.0f, 2.0f, 3.0f);
    const rectf b = rectf::from_xywh(-1.0f, 1.0f, 2.0f, 2.0f);

    const auto u = a.unite(b);
    CT_REQUIRE(catalyst::tests::nearly_equal(u.min.x, -1.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(u.min.y, 0.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(u.max.x, 2.0f));
    CT_REQUIRE(catalyst::tests::nearly_equal(u.max.y, 3.0f));

    const vec2f p{-10.0f, 10.0f};
    const auto c = a.clamp_point(p);
    CT_REQUIRE(catalyst::tests::nearly_equal(c.x, 0.0f));
    CT_REQUIRE(c.y >= 0.0f);
    CT_REQUIRE(c.y < a.max.y);
}

} // namespace

int main()
{
    test_rect_contains_and_intersection();
    test_rect_unite_and_clamp();
    return 0;
}
