/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Line breaking, the null provider, and the text content seam declared in text.hpp.
 */

#include <catalyst/text/utf8.hpp>
#include <catalyst/ui/paint.hpp>
#include <catalyst/ui/text.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace catalyst::ui
{

    // ---- null provider --------------------------------------------------------------------------

    font_metrics null_text_provider::metrics(font_id, float size_px)
    {
        return font_metrics{0.8f * size_px, 0.2f * size_px, 0.0f};
    }

    bool null_text_provider::glyph_of(font_id, float size_px, char32_t code_point, glyph &out)
    {
        if (code_point < 0x20)
            return false;

        out.code_point = code_point;
        out.advance = 0.6f * size_px;
        out.texture = no_texture;
        out.uv = rect::from_min_max(point{0.0f, 0.0f}, point{1.0f, 1.0f});
        // A space advances but has no image; everything else is a box a little narrower than its
        // advance so adjacent boxes read as separate glyphs.
        if (code_point == ' ' || code_point == 0xA0)
            out.bounds = rect{};
        else
            out.bounds = rect::from_xywh(0.05f * size_px, -0.7f * size_px, 0.5f * size_px, 0.7f * size_px);
        return true;
    }

    // ---- layout ---------------------------------------------------------------------------------

    void text_layout::clear() noexcept
    {
        glyphs.clear();
        lines.clear();
        size = extent{};
    }

    namespace
    {
        [[nodiscard]] bool is_space(char32_t cp) noexcept
        {
            return cp == ' ' || cp == '\t';
        }

        /**
         * @brief The advance width of the glyphs in `[first, last)`, trailing spaces excluded.
         */
        [[nodiscard]] float line_width(const std::vector<placed_glyph> &glyphs, std::size_t first,
                                       std::size_t last) noexcept
        {
            while (last > first && is_space(glyphs[last - 1u].g.code_point))
                --last;
            if (last == first)
                return 0.0f;
            const placed_glyph &end = glyphs[last - 1u];
            return end.position.x() + end.g.advance - glyphs[first].position.x();
        }
    } // namespace

    void layout_text(text_provider &provider, std::string_view text, const text_style &style, float font_px,
                     float max_width, text_layout &out)
    {
        out.clear();

        const float size = (style.size_px > 0.0f) ? style.size_px : font_px;
        const font_metrics fm = provider.metrics(style.font, size);
        const float line_height = (style.line_height > 0.0f) ? style.line_height : fm.line_height();
        const bool wrap = style.wrap && max_width > 0.0f && std::isfinite(max_width);

        // Pass one: place glyphs along a pen that resets at each break, recording each line as a
        // range. Wrapping moves the current word by rewinding its glyphs' x, which is cheaper than
        // buffering words and simpler than measuring twice.
        std::size_t line_start = 0;
        std::size_t word_start = 0;
        float pen = 0.0f;
        char32_t previous = 0;

        const auto finish_line = [&](std::size_t end)
        {
            out.lines.push_back(text_line{line_start, end - line_start, line_width(out.glyphs, line_start, end), 0.0f});
            line_start = end;
            previous = 0;
        };

        for (std::size_t pos = 0; pos < text.size();)
        {
            char32_t cp = catalyst::text::utf8::decode(text, pos);
            if (cp == '\r')
                continue;
            if (cp == '\n')
            {
                finish_line(out.glyphs.size());
                word_start = out.glyphs.size();
                pen = 0.0f;
                continue;
            }
            if (cp == '\t')
                cp = ' ';

            glyph g;
            if (!provider.glyph_of(style.font, size, cp, g))
                continue;

            const float kern = (previous != 0) ? provider.kerning(style.font, size, previous, cp) : 0.0f;
            pen += kern;

            if (wrap && !is_space(cp) && pen + g.advance > max_width && out.glyphs.size() > line_start)
            {
                if (word_start > line_start)
                {
                    // The word began after the line did: move the whole word down, so a word is
                    // never split when it could fit on its own line. When this glyph is the word's
                    // first, nothing has been placed yet and the word starts at the pen.
                    const float shift = (word_start < out.glyphs.size()) ? out.glyphs[word_start].position.x() : pen;
                    finish_line(word_start);
                    for (std::size_t i = word_start; i < out.glyphs.size(); ++i)
                        out.glyphs[i].position.x() -= shift;
                    pen -= shift;
                }
                else
                {
                    // The word is the whole line already: break it where it overflows.
                    finish_line(out.glyphs.size());
                    pen = 0.0f;
                }
            }

            out.glyphs.push_back(placed_glyph{g, point{pen, 0.0f}});
            pen += g.advance;
            previous = cp;

            if (is_space(cp))
                word_start = out.glyphs.size();
        }
        finish_line(out.glyphs.size());

        // Pass two: baselines, alignment and the block size. Alignment needs a reference width:
        // the wrap width when wrapping, otherwise the widest line, so unwrapped centred text is
        // centred on itself rather than on nothing.
        float widest = 0.0f;
        for (const text_line &line : out.lines)
            widest = std::max(widest, line.width);
        const float reference = wrap ? max_width : widest;

        for (std::size_t i = 0; i < out.lines.size(); ++i)
        {
            text_line &line = out.lines[i];
            line.baseline = static_cast<float>(i) * line_height + fm.ascent;

            float offset = 0.0f;
            if (style.align == text_align::center)
                offset = (reference - line.width) * 0.5f;
            else if (style.align == text_align::end)
                offset = reference - line.width;

            for (std::size_t k = line.first_glyph; k < line.first_glyph + line.glyph_count; ++k)
            {
                placed_glyph &pg = out.glyphs[k];
                pg.position = point{pg.position.x() + offset, line.baseline};
            }
        }

        out.size = extent{widest, static_cast<float>(out.lines.size()) * line_height};
    }

    extent measure_text(text_provider &provider, std::string_view text, const text_style &style, float font_px,
                        float max_width)
    {
        // A per-thread scratch so measuring, which layout calls many times a pass, does not
        // allocate a fresh vector each time.
        static thread_local text_layout scratch;
        layout_text(provider, text, style, font_px, max_width, scratch);
        return scratch.size;
    }

    // ---- content seam ---------------------------------------------------------------------------

    void text_content::attach(tree &t, node n) noexcept
    {
        t.set_measure(n, &text_content::measure, this);
        t.set_painter(n, &text_content::paint, this);
    }

    extent text_content::measure(const measure_input &input, void *user) noexcept
    {
        const text_content *content = static_cast<const text_content *>(user);
        if (content == nullptr || content->provider == nullptr)
            return extent{};

        // Wrap at whatever width is offered, definite or not: shrink-to-fit sizing hands text an
        // upper bound and expects it to fold, which is how a label in a narrow column behaves.
        const float max_width =
            (input.available_width_px > 0.0f) ? input.available_width_px : std::numeric_limits<float>::infinity();
        return measure_text(*content->provider, content->text, content->style, input.context.font_px, max_width);
    }

    void text_content::paint(const paint_context &context, batch_builder &out, void *user) noexcept
    {
        const text_content *content = static_cast<const text_content *>(user);
        if (content == nullptr || content->provider == nullptr)
            return;

        static thread_local text_layout scratch;
        const rect box = context.layout.content_box();
        const float max_width = content->style.wrap ? box.width() : std::numeric_limits<float>::infinity();
        layout_text(*content->provider, content->text, content->style, context.context.font_px, max_width, scratch);

        const color c = content->style.color.with_alpha(content->style.color.a * context.opacity);
        const texture_key previous = out.texture();
        for (const placed_glyph &pg : scratch.glyphs)
        {
            if (pg.g.bounds.is_empty())
                continue;
            const point origin = box.min + pg.position;
            const rect image = rect::from_min_max(origin + pg.g.bounds.min, origin + pg.g.bounds.max);
            out.set_texture(pg.g.texture);
            out.add_rect(image, pg.g.uv, c);
        }
        out.set_texture(previous);
    }

} // namespace catalyst::ui
