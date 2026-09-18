#include <catalyst/ui/layout.hpp>
#include <catalyst/ui/paint.hpp>
#include <catalyst/ui/text.hpp>

#include "test_common.hpp"

#include <limits>

using namespace catalyst::ui;
using catalyst::tests::near;

namespace
{
    constexpr float inf = std::numeric_limits<float>::infinity();

    // With the null provider at 10 px: advance 6, ascent 8, descent 2, line height 10.
    void test_single_line()
    {
        null_text_provider provider;
        text_layout out;
        layout_text(provider, "hello", text_style{}, 10.0f, inf, out);

        CT_REQUIRE(out.lines.size() == 1u);
        CT_REQUIRE(out.glyphs.size() == 5u);
        CT_REQUIRE(near(out.size.x(), 30.0f));
        CT_REQUIRE(near(out.size.y(), 10.0f));
        CT_REQUIRE(near(out.lines[0].width, 30.0f));
        CT_REQUIRE(near(out.lines[0].baseline, 8.0f));
        CT_REQUIRE(near(out.glyphs[0].position.x(), 0.0f));
        CT_REQUIRE(near(out.glyphs[4].position.x(), 24.0f));
        CT_REQUIRE(near(out.glyphs[4].position.y(), 8.0f));
        CT_REQUIRE(out.glyphs[1].g.code_point == U'e');

        // Empty text is still one line tall, so an empty label keeps its height.
        layout_text(provider, "", text_style{}, 10.0f, inf, out);
        CT_REQUIRE(out.lines.size() == 1u);
        CT_REQUIRE(near(out.size.x(), 0.0f));
        CT_REQUIRE(near(out.size.y(), 10.0f));

        // An explicit size overrides the node's, and line height can be forced.
        text_style big;
        big.size_px = 20.0f;
        big.line_height = 25.0f;
        layout_text(provider, "ab", big, 10.0f, inf, out);
        CT_REQUIRE(near(out.size.x(), 24.0f));
        CT_REQUIRE(near(out.size.y(), 25.0f));

        CT_REQUIRE(near(measure_text(provider, "hello", text_style{}, 10.0f, inf).x(), 30.0f));
    }

    void test_newlines_and_utf8()
    {
        null_text_provider provider;
        text_layout out;
        layout_text(provider, "ab\ncde\r\n", text_style{}, 10.0f, inf, out);
        CT_REQUIRE(out.lines.size() == 3u);
        CT_REQUIRE(out.lines[0].glyph_count == 2u);
        CT_REQUIRE(out.lines[1].glyph_count == 3u);
        CT_REQUIRE(out.lines[2].glyph_count == 0u);
        CT_REQUIRE(near(out.size.x(), 18.0f));
        CT_REQUIRE(near(out.size.y(), 30.0f));
        CT_REQUIRE(near(out.lines[1].baseline, 18.0f));
        CT_REQUIRE(near(out.glyphs[2].position.x(), 0.0f));

        // Multi-byte sequences are one glyph each; a bad byte becomes U+FFFD, not a dropped line.
        layout_text(provider, "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80\xFF", text_style{}, 10.0f, inf, out);
        CT_REQUIRE(out.glyphs.size() == 4u);
        CT_REQUIRE(out.glyphs[0].g.code_point == 0x00E9u);  // U+00E9 e-acute, two bytes
        CT_REQUIRE(out.glyphs[1].g.code_point == 0x20ACu);  // U+20AC euro sign, three bytes
        CT_REQUIRE(out.glyphs[2].g.code_point == 0x1F600u); // U+1F600 grinning face, four bytes
        CT_REQUIRE(out.glyphs[3].g.code_point == 0xFFFDu);  // the replacement for the stray 0xFF
    }

    void test_wrapping()
    {
        null_text_provider provider;
        text_layout out;

        // 6 px per glyph, 40 px wide: "aaa bbb ccc" wraps at word boundaries into three lines,
        // and the trailing space of each line does not count towards its width.
        layout_text(provider, "aaa bbb ccc", text_style{}, 10.0f, 40.0f, out);
        CT_REQUIRE(out.lines.size() == 3u);
        CT_REQUIRE(near(out.lines[0].width, 18.0f));
        CT_REQUIRE(near(out.lines[1].width, 18.0f));
        CT_REQUIRE(near(out.lines[2].width, 18.0f));
        CT_REQUIRE(near(out.size.x(), 18.0f));
        CT_REQUIRE(near(out.size.y(), 30.0f));
        // The first glyph of "bbb" sits at x = 0 on the second line.
        CT_REQUIRE(out.lines[1].first_glyph == 4u);
        CT_REQUIRE(near(out.glyphs[4].position.x(), 0.0f));
        CT_REQUIRE(near(out.glyphs[4].position.y(), 18.0f));

        // A word longer than the line breaks by character.
        layout_text(provider, "abcdefghij", text_style{}, 10.0f, 25.0f, out);
        CT_REQUIRE(out.lines.size() == 3u);
        CT_REQUIRE(out.lines[0].glyph_count == 4u);
        CT_REQUIRE(out.lines[1].glyph_count == 4u);
        CT_REQUIRE(out.lines[2].glyph_count == 2u);

        // Wrapping off: one line however narrow the width.
        text_style nowrap;
        nowrap.wrap = false;
        layout_text(provider, "aaa bbb ccc", nowrap, 10.0f, 40.0f, out);
        CT_REQUIRE(out.lines.size() == 1u);
        CT_REQUIRE(near(out.size.x(), 66.0f));

        // Alignment offsets each line within the wrap width.
        text_style centered;
        centered.align = text_align::center;
        layout_text(provider, "aa\nbbbb", centered, 10.0f, 60.0f, out);
        CT_REQUIRE(near(out.glyphs[0].position.x(), 24.0f));
        CT_REQUIRE(near(out.glyphs[2].position.x(), 18.0f));

        text_style ended;
        ended.align = text_align::end;
        layout_text(provider, "aa\nbbbb", ended, 10.0f, inf, out);
        // Unwrapped, the reference width is the widest line.
        CT_REQUIRE(near(out.glyphs[0].position.x(), 12.0f));
        CT_REQUIRE(near(out.glyphs[2].position.x(), 0.0f));
    }

    // A word whose first glyph is the one that overflows has nothing placed yet, so the wrap has to
    // start it at the pen rather than read the position of a glyph that does not exist. The layout is
    // reused after a longer string so that stale glyphs are there to be read if it does.
    void test_wrap_at_first_glyph_of_word_is_deterministic()
    {
        null_text_provider provider;
        text_layout out;
        layout_text(provider, "mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm", text_style{}, 10.0f, inf, out);

        // "aa " is 18 px wide; the first 'b' would end at 24 px, past the 20 px limit.
        layout_text(provider, "aa bb", text_style{}, 10.0f, 20.0f, out);

        CT_REQUIRE(out.lines.size() == 2u);
        CT_REQUIRE(near(out.lines[0].width, 12.0f));
        CT_REQUIRE(near(out.lines[1].width, 12.0f));
        CT_REQUIRE(near(out.glyphs[3].position.x(), 0.0f));
        CT_REQUIRE(near(out.glyphs[4].position.x(), 6.0f));
        CT_REQUIRE(near(out.size.x(), 12.0f));
        CT_REQUIRE(near(out.size.y(), 20.0f));

        // Measuring goes through a reused scratch layout too, so the same text must measure the same twice.
        const extent first = measure_text(provider, "aa bb", text_style{}, 10.0f, 20.0f);
        const extent second = measure_text(provider, "aa bb", text_style{}, 10.0f, 20.0f);
        CT_REQUIRE(near(first.y(), 20.0f));
        CT_REQUIRE(near(second.y(), first.y()));
    }

    void test_content_measures_and_paints()
    {
        null_text_provider provider;
        text_content content;
        content.text = "aaa bbb ccc";
        content.provider = &provider;
        content.style.color = colors::red;

        tree t;
        const node root = t.create();
        t.mutable_style(root).width = px(40.0f);
        t.mutable_style(root).direction = flex_direction::column;
        t.mutable_style(root).font_size = px(10.0f);

        const node label = t.create_child(root);
        content.attach(t, label);
        CT_REQUIRE(t.measure_of(label) == &text_content::measure);
        CT_REQUIRE(t.painter_of(label) == &text_content::paint);

        layout(t, root, layout_params::for_viewport(extent{40.0f, 200.0f}));

        // The label wrapped to the column's width: three lines of 10 px, 18 px wide.
        const rect box = t.layout_of(label).border_box();
        CT_REQUIRE(near(box.width(), 40.0f)); // stretched by the column
        CT_REQUIRE(near(box.height(), 30.0f));

        render_batch batch;
        batch_builder b{batch};
        paint(t, root, b, paint_params::for_viewport(extent{40.0f, 200.0f}));

        // Nine letters painted as boxes, spaces skipped, all in the null provider's untextured command.
        CT_REQUIRE(batch.vertices.size() == 36u);
        CT_REQUIRE(batch.commands.size() == 1u);
        CT_REQUIRE(batch.commands[0].texture == no_texture);
        CT_REQUIRE(batch.vertices[0].color == colors::red.to_rgba8());
        // The first glyph's box sits at the content origin plus the glyph's own offset, above the baseline.
        CT_REQUIRE(near(batch.vertices[0].position.x(), 0.5f));
        CT_REQUIRE(near(batch.vertices[0].position.y(), 8.0f - 7.0f));
        CT_REQUIRE(b.texture() == no_texture);

        // A wider column: one line.
        t.mutable_style(root).width = px(100.0f);
        layout(t, root, layout_params::for_viewport(extent{100.0f, 200.0f}));
        CT_REQUIRE(near(t.layout_of(label).border_box().height(), 10.0f));

        // No provider: measures as nothing and paints nothing.
        content.provider = nullptr;
        layout(t, root, layout_params::for_viewport(extent{100.0f, 200.0f}));
        CT_REQUIRE(near(t.layout_of(label).border_box().height(), 0.0f));
        batch.clear();
        paint(t, root, b, paint_params::for_viewport(extent{100.0f, 200.0f}));
        CT_REQUIRE(batch.empty());
    }

} // namespace

int main()
{
    test_single_line();
    test_newlines_and_utf8();
    test_wrapping();
    test_wrap_at_first_glyph_of_word_is_deterministic();
    test_content_measures_and_paints();
    return 0;
}
