/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The text seam: what a font engine must provide, and the line layout built on it.
 */

#pragma once

#include <catalyst/ui/batch.hpp>
#include <catalyst/ui/color.hpp>
#include <catalyst/ui/geometry.hpp>
#include <catalyst/ui/node.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace catalyst::ui
{

    /**
     * @struct font_id
     * @brief Names a font face to a `text_provider`. Zero is the provider's default face.
     */
    struct font_id
    {
        std::uint32_t value = 0;

        [[nodiscard]] constexpr bool operator==(const font_id &other) const noexcept = default;
    };

    /**
     * @struct font_metrics
     * @brief Vertical metrics of a face at one size, in pixels.
     */
    struct font_metrics
    {
        /** @brief Distance from the baseline up to the top of the tallest glyph. Positive. */
        float ascent = 0.0f;
        /** @brief Distance from the baseline down to the bottom of the deepest glyph. Positive. */
        float descent = 0.0f;
        /** @brief Extra space the face asks for between lines. */
        float line_gap = 0.0f;

        /** @brief The default distance between baselines. */
        [[nodiscard]] constexpr float line_height() const noexcept { return ascent + descent + line_gap; }
    };

    /**
     * @struct glyph
     * @brief One glyph as a provider reports it: how far to advance, where its image sits, and where to find it.
     */
    struct glyph
    {
        /** @brief The code point this glyph renders. */
        char32_t code_point = 0;
        /** @brief Horizontal advance to the next glyph, in pixels. */
        float advance = 0.0f;
        /**
         * @brief The glyph's image rectangle relative to the pen at the baseline, y down, so the
         * top of a capital letter has a negative `min.y()`. Empty for a glyph with no image, such as a space.
         */
        rect bounds{};
        /** @brief Texture coordinates of the image within `texture`. */
        rect uv{};
        /** @brief The atlas holding the image, or `no_texture` for a provider without one. */
        texture_key texture = no_texture;
    };

    /**
     * @class text_provider
     * @brief What the text layout needs from a font engine. Implemented by a real font stack later, and by
     * `null_text_provider` now.
     * @details Three questions -- how tall is a line, what does this code point look like, do these
     * two glyphs kern -- and nothing about files, rasterisation or atlases, which are the
     * provider's own business. Shaping (ligatures, complex scripts) is deliberately absent from
     * the seam: a shaping provider would expose a run-level call, and adding it later does not
     * change these three.
     */
    class text_provider
    {
    public:
        virtual ~text_provider() = default;

        /** @brief The vertical metrics of a face at a size. */
        [[nodiscard]] virtual font_metrics metrics(font_id font, float size_px) = 0;

        /**
         * @brief Looks a glyph up.
         * @param font The face.
         * @param size_px The size.
         * @param code_point The code point.
         * @param out Receives the glyph.
         * @return False when the face has no glyph for the code point and nothing should be drawn or advanced.
         */
        [[nodiscard]] virtual bool glyph_of(font_id font, float size_px, char32_t code_point, glyph &out) = 0;

        /** @brief The horizontal adjustment between two glyphs, in pixels. Zero by default. */
        [[nodiscard]] virtual float kerning(font_id, float, char32_t, char32_t) { return 0.0f; }
    };

    /**
     * @class null_text_provider
     * @brief A provider with no font: every glyph is a box, monospaced from the size.
     * @details What the layout and paint tests run against, and what an application gets before a
     * real provider is wired in: text takes up plausible space and paints as "tofu" boxes rather
     * than vanishing, which is easier to debug than nothing. Metrics are the usual proportions of a
     * Latin face: ascent 0.8 em, descent 0.2 em, advance 0.6 em.
     */
    class null_text_provider final : public text_provider
    {
    public:
        [[nodiscard]] font_metrics metrics(font_id font, float size_px) override;
        [[nodiscard]] bool glyph_of(font_id font, float size_px, char32_t code_point, glyph &out) override;
    };

    /**
     * @enum text_align
     * @brief Horizontal alignment of lines within the available width.
     */
    enum class text_align : std::uint8_t
    {
        start = 0,
        center,
        end,
    };

    /**
     * @struct text_style
     * @brief How a run of text is set.
     */
    struct text_style
    {
        /** @brief The face. */
        font_id font{};
        /** @brief The size in pixels. Zero means the node's resolved `font_size`. */
        float size_px = 0.0f;
        /** @brief Distance between baselines in pixels. Zero means the face's own. */
        float line_height = 0.0f;
        /** @brief Alignment within the width offered. */
        text_align align = text_align::start;
        /** @brief Whether lines wrap at the width offered. Off, the text is one line per newline. */
        bool wrap = true;
        /** @brief The colour glyphs are drawn in. */
        ui::color color = colors::black;
    };

    /**
     * @struct placed_glyph
     * @brief A glyph and where its pen sits, relative to the top-left of the text block.
     */
    struct placed_glyph
    {
        glyph g{};
        /** @brief The pen position at the baseline. The image is at `position + g.bounds`. */
        point position{};
    };

    /**
     * @struct text_line
     * @brief One line of a laid-out block.
     */
    struct text_line
    {
        /** @brief Index of the line's first glyph in `text_layout::glyphs`. */
        std::size_t first_glyph = 0;
        /** @brief How many glyphs the line has. */
        std::size_t glyph_count = 0;
        /** @brief The line's advance width, trailing spaces excluded. */
        float width = 0.0f;
        /** @brief The baseline's distance from the top of the block. */
        float baseline = 0.0f;
    };

    /**
     * @struct text_layout
     * @brief The result of `layout_text`: positioned glyphs, their lines, and the block's size.
     */
    struct text_layout
    {
        std::vector<placed_glyph> glyphs;
        std::vector<text_line> lines;
        /** @brief Width of the widest line and height of all lines. */
        extent size{};

        /** @brief Empties the layout without releasing its storage. */
        void clear() noexcept;
    };

    /**
     * @brief Breaks UTF-8 text into lines and positions every glyph.
     * @details Greedy line breaking: a word that does not fit moves whole to the next line, a word
     * longer than a line breaks where it overflows, and a newline always breaks. Tabs are spaces.
     * Malformed UTF-8 decodes to U+FFFD. The text always has at least one line, so an empty label
     * is still one line tall.
     * @param provider The font engine.
     * @param text The UTF-8 text.
     * @param style How to set it.
     * @param font_px The size to use when `style.size_px` is zero.
     * @param max_width The width to wrap at, in pixels. Infinity, or a non-positive value, means no wrapping.
     * @param out The layout to fill. Cleared first.
     */
    void layout_text(text_provider &provider, std::string_view text, const text_style &style, float font_px,
                     float max_width, text_layout &out);

    /**
     * @brief The size `layout_text` would produce, without keeping the glyphs.
     */
    [[nodiscard]] extent measure_text(text_provider &provider, std::string_view text, const text_style &style,
                                      float font_px, float max_width);

    /**
     * @struct text_content
     * @brief Text attached to a node: the string, its style, and the provider that sets it.
     * @details Owned by the application and registered on a node with `attach`, which wires both
     * halves of the content seam -- `measure_fn` so layout can size the node from the text, and
     * `paint_fn` so the paint pass draws it. The struct must outlive the node's use of it; the tree
     * holds only a pointer.
     */
    struct text_content
    {
        /** @brief The UTF-8 text. */
        std::string text;
        /** @brief How it is set. */
        text_style style{};
        /** @brief The font engine. Text with no provider measures as empty and paints nothing. */
        text_provider *provider = nullptr;

        /**
         * @brief Registers this content's measure and paint callbacks on a node.
         * @param t The tree.
         * @param n The node. It becomes a leaf for layout purposes.
         */
        void attach(tree &t, node n) noexcept;

        /** @brief The `measure_fn` `attach` registers. `user` is the `text_content`. */
        static extent measure(const measure_input &input, void *user) noexcept;

        /** @brief The `paint_fn` `attach` registers. `user` is the `text_content`. */
        static void paint(const paint_context &context, batch_builder &out, void *user) noexcept;
    };

} // namespace catalyst::ui
