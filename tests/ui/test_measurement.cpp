#include <catalyst/ui/measurement.hpp>

#include "../math/test_common.hpp"

namespace
{

void test_length_resolve_absolute_and_relative()
{
    catalyst::ui::resolve_context ctx{};
    ctx.dpi_scale = 2.0f;
    ctx.font_px = 20.0f;
    ctx.root_font_px = 16.0f;
    ctx.parent_width_px = 200.0f;
    ctx.parent_height_px = 100.0f;
    ctx.viewport_width_px = 1000.0f;
    ctx.viewport_height_px = 500.0f;

    const auto v = catalyst::ui::px(10.0f) + catalyst::ui::dp(3.0f) + catalyst::ui::em(2.0f) + catalyst::ui::rem(1.0f);
    const float px = catalyst::ui::resolve_or(v, catalyst::ui::axis::x, ctx);

    // 10px + 3dp*2 + 2em*20 + 1rem*16 = 10 + 6 + 40 + 16 = 72
    CT_REQUIRE(catalyst::tests::nearly_equal(px, 72.0f));
}

void test_length_resolve_percent_vw_vh_and_axis()
{
    catalyst::ui::resolve_context ctx{};
    ctx.parent_width_px = 200.0f;
    ctx.parent_height_px = 100.0f;
    ctx.viewport_width_px = 1000.0f;
    ctx.viewport_height_px = 500.0f;

    const auto v = catalyst::ui::percent(50.0f) + catalyst::ui::vw(10.0f) + catalyst::ui::vh(10.0f);

    // x axis: 50% of parent width (100) + 10% viewport width (100) + 10% viewport height (50) = 250
    const float x = catalyst::ui::resolve_or(v, catalyst::ui::axis::x, ctx);
    CT_REQUIRE(catalyst::tests::nearly_equal(x, 250.0f));

    // y axis: 50% of parent height (50) + 10% viewport width (100) + 10% viewport height (50) = 200
    const float y = catalyst::ui::resolve_or(v, catalyst::ui::axis::y, ctx);
    CT_REQUIRE(catalyst::tests::nearly_equal(y, 200.0f));
}

void test_length_auto_fallback()
{
    catalyst::ui::resolve_context ctx{};
    const auto v = catalyst::ui::auto_();

    CT_REQUIRE(catalyst::tests::nearly_equal(catalyst::ui::resolve_or(v, catalyst::ui::axis::x, ctx, 123.0f), 123.0f));
}

void test_length_calc_style_arithmetic()
{
    catalyst::ui::resolve_context ctx{};
    ctx.parent_width_px = 200.0f;

    // calc(50% - 10px)
    const auto v = catalyst::ui::percent(50.0f) - catalyst::ui::px(10.0f);
    const float x = catalyst::ui::resolve_or(v, catalyst::ui::axis::x, ctx);

    CT_REQUIRE(catalyst::tests::nearly_equal(x, 90.0f));
}

} // namespace

int main()
{
    test_length_resolve_absolute_and_relative();
    test_length_resolve_percent_vw_vh_and_axis();
    test_length_auto_fallback();
    test_length_calc_style_arithmetic();
    return 0;
}
