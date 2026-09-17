#include <catalyst/ui/hit_test.hpp>
#include <catalyst/ui/layout.hpp>

#include "test_common.hpp"

using namespace catalyst::ui;

namespace
{
    void run(tree &t, node root, float w, float h)
    {
        layout(t, root, layout_params::for_viewport(extent{w, h}));
    }

    node sized(tree &t, node parent, float w, float h)
    {
        const node n = t.create_child(parent);
        t.mutable_style(n).width = px(w);
        t.mutable_style(n).height = px(h);
        return n;
    }

    void test_deepest_and_topmost()
    {
        tree t;
        const node root = t.create();
        t.mutable_style(root).width = px(200.0f);
        t.mutable_style(root).height = px(100.0f);

        const node a = sized(t, root, 100.0f, 100.0f);
        const node a_inner = sized(t, a, 50.0f, 50.0f);
        const node b = sized(t, root, 100.0f, 100.0f);
        run(t, root, 200.0f, 100.0f);

        CT_REQUIRE(hit_test(t, root, point{10.0f, 10.0f}) == a_inner);
        CT_REQUIRE(hit_test(t, root, point{75.0f, 75.0f}) == a);
        CT_REQUIRE(hit_test(t, root, point{150.0f, 50.0f}) == b);
        CT_REQUIRE(is_null(hit_test(t, root, point{250.0f, 50.0f})));
        CT_REQUIRE(is_null(hit_test(t, root, point{-1.0f, 50.0f})));
        CT_REQUIRE(is_null(hit_test(t, null_node, point{10.0f, 10.0f})));

        // Half-open boxes: the far edge belongs to the neighbour.
        CT_REQUIRE(hit_test(t, root, point{100.0f, 10.0f}) == b);
        CT_REQUIRE(hit_test(t, root, point{99.9f, 10.0f}) == a);

        // Overlapping siblings: the later child is painted on top and wins.
        tree o;
        const node oroot = o.create();
        o.mutable_style(oroot).width = px(100.0f);
        o.mutable_style(oroot).height = px(100.0f);
        const node under = sized(o, oroot, 100.0f, 100.0f);
        o.mutable_style(under).position = position_mode::absolute;
        const node over = sized(o, oroot, 100.0f, 100.0f);
        o.mutable_style(over).position = position_mode::absolute;
        run(o, oroot, 100.0f, 100.0f);
        CT_REQUIRE(hit_test(o, oroot, point{50.0f, 50.0f}) == over);
        o.mutable_style(over).display = display_mode::none;
        run(o, oroot, 100.0f, 100.0f);
        CT_REQUIRE(hit_test(o, oroot, point{50.0f, 50.0f}) == under);
    }

    void test_pointer_events_none()
    {
        tree t;
        const node root = t.create();
        t.mutable_style(root).width = px(100.0f);
        t.mutable_style(root).height = px(100.0f);

        // An overlay that lets clicks through, except on its own button.
        const node overlay = sized(t, root, 100.0f, 100.0f);
        t.mutable_style(overlay).position = position_mode::absolute;
        t.mutable_style(overlay).pointer_events = pointer_mode::none;
        const node button = sized(t, overlay, 20.0f, 20.0f);
        run(t, root, 100.0f, 100.0f);

        CT_REQUIRE(hit_test(t, root, point{10.0f, 10.0f}) == button);
        CT_REQUIRE(hit_test(t, root, point{50.0f, 50.0f}) == root);
    }

    void test_overflow_clips_hits()
    {
        tree t;
        const node root = t.create();
        t.mutable_style(root).width = px(100.0f);
        t.mutable_style(root).height = px(100.0f);
        t.mutable_style(root).border = edges_length::all(px(10.0f));

        const node child = sized(t, root, 300.0f, 300.0f);
        t.mutable_style(child).position = position_mode::absolute;
        t.mutable_style(child).inset = edges_length{px(0.0f), px(0.0f), auto_(), auto_()};
        run(t, root, 100.0f, 100.0f);

        // Visible overflow: the child is hittable where it hangs out of the parent.
        CT_REQUIRE(hit_test(t, root, point{200.0f, 200.0f}) == child);
        CT_REQUIRE(hit_test(t, root, point{50.0f, 50.0f}) == child);

        // Hidden overflow: outside the padding box the child is unreachable, and in the border ring
        // the parent itself is hit.
        t.mutable_style(root).overflow = overflow_mode::hidden;
        run(t, root, 100.0f, 100.0f);
        CT_REQUIRE(is_null(hit_test(t, root, point{200.0f, 200.0f})));
        CT_REQUIRE(hit_test(t, root, point{5.0f, 50.0f}) == root);
        CT_REQUIRE(hit_test(t, root, point{50.0f, 50.0f}) == child);
    }

} // namespace

int main()
{
    test_deepest_and_topmost();
    test_pointer_events_none();
    test_overflow_clips_hits();
    return 0;
}
