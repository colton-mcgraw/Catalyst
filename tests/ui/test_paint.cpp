#include <catalyst/ui/layout.hpp>
#include <catalyst/ui/paint.hpp>

#include "test_common.hpp"

using namespace catalyst::ui;
using catalyst::tests::near;

namespace
{
    void run(tree &t, node root, float w, float h)
    {
        layout(t, root, layout_params::for_viewport(extent{w, h}));
    }

    rect box(float x, float y, float w, float h)
    {
        return rect::from_xywh(x, y, w, h);
    }

    int painter_calls = 0;
    float painter_opacity = 0.0f;
    rect painter_content{};

    void custom_painter(const paint_context &ctx, batch_builder &out, void *user) noexcept
    {
        ++painter_calls;
        painter_opacity = ctx.opacity;
        painter_content = ctx.layout.content_box();
        CT_REQUIRE(user == &painter_calls);
        out.add_rect(ctx.layout.content_box(), colors::green);
    }

    void test_background_border_and_order()
    {
        tree t;
        const node root = t.create();
        t.mutable_style(root).width = px(200.0f);
        t.mutable_style(root).height = px(100.0f);
        t.mutable_style(root).background = colors::white;

        const node child = t.create_child(root);
        t.mutable_style(child).width = px(50.0f);
        t.mutable_style(child).height = px(50.0f);
        t.mutable_style(child).background = colors::red;
        t.mutable_style(child).border = edges_length::all(px(2.0f));
        t.mutable_style(child).border_color = colors::black;

        run(t, root, 200.0f, 100.0f);

        render_batch batch;
        batch_builder b{batch};
        paint(t, root, b, paint_params::for_viewport(extent{200.0f, 100.0f}));

        // Root background (4), child background (4), child border ring (8): one merged command.
        CT_REQUIRE(batch.vertices.size() == 16u);
        CT_REQUIRE(batch.commands.size() == 1u);
        CT_REQUIRE(batch.vertices[0].color == colors::white.to_rgba8());
        CT_REQUIRE(batch.vertices[4].color == colors::red.to_rgba8());
        CT_REQUIRE(batch.vertices[8].color == colors::black.to_rgba8());
        // The child's background covers its border box, painted after the root's.
        CT_REQUIRE(near(batch.vertices[6].position.x(), 50.0f));
        CT_REQUIRE(near(batch.vertices[6].position.y(), 50.0f));

        // A transparent, borderless node emits nothing but still paints its children.
        t.mutable_style(root).background = colors::transparent;
        run(t, root, 200.0f, 100.0f);
        batch.clear();
        paint(t, root, b, paint_params::for_viewport(extent{200.0f, 100.0f}));
        CT_REQUIRE(batch.vertices.size() == 12u);
    }

    void test_rounded_corners_resolve_against_the_box()
    {
        tree t;
        const node root = t.create();
        t.mutable_style(root).width = px(100.0f);
        t.mutable_style(root).height = px(100.0f);
        t.mutable_style(root).background = colors::blue;
        t.mutable_style(root).border_radius = corners_length::all(percent(10.0f));
        run(t, root, 100.0f, 100.0f);

        render_batch batch;
        batch_builder b{batch};
        paint(t, root, b, paint_params::for_viewport(extent{100.0f, 100.0f}));

        // 10% of a 100 px box is a 10 px radius: the first outline point is at (0, 10).
        CT_REQUIRE(batch.vertices.size() > 4u);
        CT_REQUIRE(near(batch.vertices[1].position.x(), 0.0f));
        CT_REQUIRE(near(batch.vertices[1].position.y(), 10.0f));
    }

    void test_overflow_clips_children()
    {
        tree t;
        const node root = t.create();
        t.mutable_style(root).width = px(100.0f);
        t.mutable_style(root).height = px(100.0f);
        t.mutable_style(root).overflow = overflow_mode::hidden;
        // A border, not padding: overflow clips to the padding box, which is inset by the border.
        // Content in the padding is inside the clip, as in CSS.
        t.mutable_style(root).border = edges_length::all(px(10.0f));

        const node inside = t.create_child(root);
        t.mutable_style(inside).position = position_mode::absolute;
        t.mutable_style(inside).inset = edges_length{px(0.0f), px(0.0f), auto_(), auto_()};
        t.mutable_style(inside).width = px(20.0f);
        t.mutable_style(inside).height = px(20.0f);
        t.mutable_style(inside).background = colors::red;

        const node outside = t.create_child(root);
        t.mutable_style(outside).position = position_mode::absolute;
        t.mutable_style(outside).inset = edges_length{px(500.0f), px(500.0f), auto_(), auto_()};
        t.mutable_style(outside).width = px(20.0f);
        t.mutable_style(outside).height = px(20.0f);
        t.mutable_style(outside).background = colors::red;

        run(t, root, 100.0f, 100.0f);

        render_batch batch;
        batch_builder b{batch};
        paint(t, root, b, paint_params::for_viewport(extent{100.0f, 100.0f}));

        // Only the inside child is emitted, with the padding box as its clip; the outside one is
        // culled entirely.
        CT_REQUIRE(batch.vertices.size() == 4u);
        CT_REQUIRE(batch.commands.size() == 1u);
        CT_REQUIRE(batch.commands[0].clip == box(10.0f, 10.0f, 80.0f, 80.0f));

        // Painting again after the clip shows the builder's stack was restored.
        b.add_rect(box(0, 0, 1, 1), colors::red);
        CT_REQUIRE(batch.commands.size() == 2u);
        CT_REQUIRE(!batch.commands[1].clip.is_empty());
        CT_REQUIRE(batch.commands[1].clip.min.x() < 0.0f);
    }

    /** @brief Paint parameters for the 100x100 viewport with per-node alpha scaling rather than layers. */
    paint_params multiplied()
    {
        paint_params p = paint_params::for_viewport(extent{100.0f, 100.0f});
        p.compositing = opacity_mode::multiply;
        return p;
    }

    void test_opacity_and_hidden()
    {
        tree t;
        const node root = t.create();
        t.mutable_style(root).width = px(100.0f);
        t.mutable_style(root).height = px(100.0f);
        t.mutable_style(root).opacity = 0.5f;

        const node child = t.create_child(root);
        t.mutable_style(child).width = px(10.0f);
        t.mutable_style(child).height = px(10.0f);
        t.mutable_style(child).background = colors::white;
        t.mutable_style(child).opacity = 0.5f;

        const node hidden = t.create_child(root);
        t.mutable_style(hidden).width = px(10.0f);
        t.mutable_style(hidden).height = px(10.0f);
        t.mutable_style(hidden).background = colors::white;
        t.mutable_style(hidden).display = display_mode::none;

        run(t, root, 100.0f, 100.0f);

        render_batch batch;
        batch_builder b{batch};
        paint(t, root, b, multiplied());

        // Opacity compounds: 0.5 * 0.5 = 0.25 on the child's alpha; the hidden node is skipped.
        CT_REQUIRE(batch.vertices.size() == 4u);
        CT_REQUIRE((batch.vertices[0].color >> 24) == 64u);

        // A fully transparent subtree is skipped without visiting it.
        t.mutable_style(root).opacity = 0.0f;
        run(t, root, 100.0f, 100.0f);
        batch.clear();
        paint(t, root, b, multiplied());
        CT_REQUIRE(batch.empty());

        // Nothing painted for an invalid root or a tree that was never laid out.
        tree fresh;
        const node unlaid = fresh.create();
        fresh.mutable_style(unlaid).background = colors::red;
        paint(fresh, unlaid, b, paint_params{});
        paint(fresh, null_node, b, paint_params{});
        CT_REQUIRE(batch.empty());
    }

    void test_painter_seam()
    {
        tree t;
        const node root = t.create();
        t.mutable_style(root).width = px(100.0f);
        t.mutable_style(root).height = px(100.0f);
        t.mutable_style(root).padding = edges_length::all(px(10.0f));
        t.mutable_style(root).background = colors::white;
        t.mutable_style(root).opacity = 0.5f;
        t.set_painter(root, &custom_painter, &painter_calls);
        CT_REQUIRE(t.painter_of(root) == &custom_painter);
        CT_REQUIRE(t.painter_user(root) == &painter_calls);

        const node child = t.create_child(root);
        t.mutable_style(child).width = px(10.0f);
        t.mutable_style(child).height = px(10.0f);
        t.mutable_style(child).background = colors::red;

        run(t, root, 100.0f, 100.0f);

        render_batch batch;
        batch_builder b{batch};
        paint(t, root, b, multiplied());

        // Background, then the painter's content, then the child: green sits between white and red.
        CT_REQUIRE(painter_calls == 1);
        CT_REQUIRE(near(painter_opacity, 0.5f));
        CT_REQUIRE(painter_content == box(10.0f, 10.0f, 80.0f, 80.0f));
        CT_REQUIRE(batch.vertices.size() == 12u);
        CT_REQUIRE(batch.vertices[4].color == colors::green.to_rgba8());
        CT_REQUIRE(batch.vertices[8].color == colors::red.with_alpha(0.5f).to_rgba8());

        // Clearing the painter stops the calls; destroying the node drops it too.
        t.set_painter(root, nullptr);
        CT_REQUIRE(t.painter_of(root) == nullptr);
        batch.clear();
        paint(t, root, b, multiplied());
        CT_REQUIRE(painter_calls == 1);
    }

    void test_group_opacity_layers()
    {
        tree t;
        const node root = t.create();
        t.mutable_style(root).width = px(100.0f);
        t.mutable_style(root).height = px(100.0f);
        t.mutable_style(root).background = colors::white;

        const node group = t.create_child(root);
        t.mutable_style(group).width = px(50.0f);
        t.mutable_style(group).height = px(50.0f);
        t.mutable_style(group).background = colors::red;
        t.mutable_style(group).opacity = 0.5f;
        t.set_painter(group, &custom_painter, &painter_calls);

        const node child = t.create_child(group);
        t.mutable_style(child).width = px(10.0f);
        t.mutable_style(child).height = px(10.0f);
        t.mutable_style(child).background = colors::blue;
        t.mutable_style(child).opacity = 0.5f;

        run(t, root, 100.0f, 100.0f);

        render_batch batch;
        batch_builder b{batch};
        paint(t, root, b, paint_params::for_viewport(extent{100.0f, 100.0f})); // group is the default

        // One layer per translucent node, nested, over the node's subtree bounds; nothing inside a
        // layer has its alpha scaled, the painter included.
        CT_REQUIRE(batch.layers.size() == 2u);
        CT_REQUIRE(batch.layers[0].parent == no_layer);
        CT_REQUIRE(batch.layers[1].parent == 0u);
        CT_REQUIRE(near(batch.layers[0].opacity, 0.5f));
        CT_REQUIRE(batch.layers[0].bounds == t.layout_of(group).subtree_bounds);
        CT_REQUIRE(batch.layers[0].bounds == box(0.0f, 0.0f, 50.0f, 50.0f));
        CT_REQUIRE(near(painter_opacity, 1.0f));

        // Root background outside any layer; the group's background and painter share a command in
        // layer 0; the child is alone in layer 1, and the layers' ranges nest.
        CT_REQUIRE(batch.commands.size() == 3u);
        CT_REQUIRE(batch.commands[0].layer == no_layer);
        CT_REQUIRE(batch.commands[1].layer == 0u);
        CT_REQUIRE(batch.commands[2].layer == 1u);
        CT_REQUIRE(batch.vertices[4].color == colors::red.to_rgba8());
        CT_REQUIRE(batch.vertices[8].color == colors::green.to_rgba8());
        CT_REQUIRE(batch.vertices[12].color == colors::blue.to_rgba8());
        CT_REQUIRE(batch.layers[0].first_command == 1u);
        CT_REQUIRE(batch.layers[0].end_command == 3u);
        CT_REQUIRE(batch.layers[1].first_command == 2u);
        CT_REQUIRE(batch.layers[1].end_command == 3u);

        // The root opacity is a layer over everything, and the others nest inside it.
        batch.clear();
        paint_params dimmed = paint_params::for_viewport(extent{100.0f, 100.0f});
        dimmed.opacity = 0.25f;
        paint(t, root, b, dimmed);
        CT_REQUIRE(batch.layers.size() == 3u);
        CT_REQUIRE(batch.layers[0].parent == no_layer);
        CT_REQUIRE(near(batch.layers[0].opacity, 0.25f));
        CT_REQUIRE(batch.layers[0].first_command == 0u);
        CT_REQUIRE(batch.layers[0].end_command == batch.commands.size());
        CT_REQUIRE(batch.layers[1].parent == 0u);
        CT_REQUIRE(batch.commands[0].layer == 0u);
        CT_REQUIRE(batch.vertices[0].color == colors::white.to_rgba8());

        // Multiply mode opens no layer for the same tree.
        batch.clear();
        paint(t, root, b, multiplied());
        CT_REQUIRE(batch.layers.empty());
        CT_REQUIRE(batch.commands.size() == 1u);
        CT_REQUIRE(batch.vertices[4].color == colors::red.with_alpha(0.5f).to_rgba8());

        // A zero opacity still skips the subtree, with no layer.
        batch.clear();
        t.mutable_style(group).opacity = 0.0f;
        run(t, root, 100.0f, 100.0f);
        paint(t, root, b, paint_params::for_viewport(extent{100.0f, 100.0f}));
        CT_REQUIRE(batch.layers.empty());
        CT_REQUIRE(batch.commands.size() == 1u);
    }

} // namespace

int main()
{
    test_background_border_and_order();
    test_rounded_corners_resolve_against_the_box();
    test_overflow_clips_children();
    test_opacity_and_hidden();
    test_painter_seam();
    test_group_opacity_layers();
    return 0;
}
