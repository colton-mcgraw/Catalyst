/*
 * @file font_provider.hpp
 * @brief A `ui::text_provider` over a TrueType file, rasterising glyphs through stb_truetype into a single-channel
 * atlas the application uploads and registers with `ui::renderer`.
 * @details The UI module's text seam asks a provider three questions -- line metrics, one glyph, one kerning pair --
 * and leaves files, rasterisation and atlases to the provider. This is the smallest provider that answers them with a
 * real face. Glyphs are rasterised the first time they are asked for at a size, packed into a CPU-side `r8` bitmap
 * with a shelf packer, and cached; the bitmap is what the sample turns into a `rendering::texture` and registers under
 * `key()`, which every glyph's `texture` field carries. The renderer treats an `r8_unorm` texture as a coverage mask,
 * so the glyph's colour is the vertex colour and the atlas only supplies alpha.
 *
 * The provider says when the sample has work to do: `take_dirty` is true when pixels changed since the last upload,
 * and `take_resized` when the atlas grew, which invalidates every `uv` handed out before, so a batch painted during
 * that growth must be painted again. Sizes are quantised to whole pixels for the cache; the module asks with floats.
 * License: MIT (see LICENSE).
 */

#pragma once

#include <catalyst/ui/text.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace ui_sample
{

    /** @brief A `text_provider` backed by one TrueType (or OpenType with TrueType outlines) face. */
    class ttf_text_provider final : public catalyst::ui::text_provider
    {
    public:
        /** @brief What the atlas starts as and how far it may grow. */
        struct desc
        {
            /** @brief Starting atlas width and height in pixels. Grows by doubling as glyphs arrive. */
            std::uint32_t initial_size = 256;
            /** @brief The atlas never grows past this on either axis; glyphs that do not fit then draw nothing. */
            std::uint32_t max_size = 4096;
            /** @brief Empty pixels kept around each glyph so linear filtering never bleeds a neighbour in. */
            std::uint32_t padding = 1;
            /** @brief The `texture_key` stamped on every glyph. Must not be `no_texture`. */
            catalyst::ui::texture_key key = 1;
        };

        /** @brief A provider with the default `desc`. */
        ttf_text_provider();
        explicit ttf_text_provider(const desc &d);
        ~ttf_text_provider() override;

        ttf_text_provider(const ttf_text_provider &) = delete;
        ttf_text_provider &operator=(const ttf_text_provider &) = delete;

        /**
         * @brief Reads a font file and takes face `index` from it (a `.ttc` holds several; a `.ttf` holds one).
         * @return True on success. On failure `error()` says why, and the provider stays as it was.
         */
        bool open(const std::filesystem::path &file, int index = 0);

        /** @brief Adopts font bytes already in memory. The provider keeps its own copy. */
        bool open(std::span<const std::byte> bytes, int index = 0);

        /** @brief True once a face has been opened. Before that every glyph lookup fails. */
        [[nodiscard]] bool valid() const noexcept { return face_ != nullptr; }

        /** @brief The last failure `open` reported. */
        [[nodiscard]] const std::string &error() const noexcept { return error_; }

        // ---- text_provider ----------------------------------------------------------------------

        [[nodiscard]] catalyst::ui::font_metrics metrics(catalyst::ui::font_id font, float size_px) override;
        [[nodiscard]] bool glyph_of(catalyst::ui::font_id font, float size_px, char32_t code_point,
                                    catalyst::ui::glyph &out) override;
        [[nodiscard]] float kerning(catalyst::ui::font_id font, float size_px, char32_t left, char32_t right) override;

        // ---- the atlas --------------------------------------------------------------------------

        /** @brief The key glyphs carry; register the uploaded atlas under it. */
        [[nodiscard]] catalyst::ui::texture_key key() const noexcept { return desc_.key; }

        /** @brief The atlas pixels, one byte of coverage per pixel, rows tightly packed. */
        [[nodiscard]] std::span<const std::byte> pixels() const noexcept;

        [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
        [[nodiscard]] std::uint32_t height() const noexcept { return height_; }

        /** @brief True once per change: pixels were written since the last call, so the texture needs uploading. */
        [[nodiscard]] bool take_dirty() noexcept;

        /**
         * @brief True once per growth: the atlas was reallocated larger since the last call. Texture coordinates
         * handed out before it grew point at the old size, so anything painted since the previous call must be
         * painted again after the upload.
         */
        [[nodiscard]] bool take_resized() noexcept;

        /** @brief How many glyph images were dropped because the atlas hit `max_size`. */
        [[nodiscard]] std::uint32_t dropped() const noexcept { return dropped_; }

        /** @brief How many distinct (size, code point) glyphs are cached. */
        [[nodiscard]] std::size_t cached() const noexcept { return cache_.size(); }

    private:
        struct face; ///< Wraps the stb_truetype font info so the header stays free of stb.

        /** @brief What one rasterised glyph came out as. Pixel positions, not texture coordinates. */
        struct entry
        {
            float advance = 0.0f;
            float bounds_x = 0.0f; ///< Image offset from the pen, y down; negative y is above the baseline.
            float bounds_y = 0.0f;
            std::uint32_t x = 0; ///< Where the image sits in the atlas. Zero size means no image.
            std::uint32_t y = 0;
            std::uint32_t w = 0;
            std::uint32_t h = 0;
        };

        struct cache_key
        {
            std::uint32_t size = 0;
            char32_t code_point = 0;
            [[nodiscard]] bool operator==(const cache_key &) const noexcept = default;
        };

        struct cache_hash
        {
            [[nodiscard]] std::size_t operator()(const cache_key &k) const noexcept
            {
                return (static_cast<std::size_t>(k.size) << 32) ^ static_cast<std::size_t>(k.code_point);
            }
        };

        [[nodiscard]] static std::uint32_t quantise(float size_px) noexcept;
        [[nodiscard]] float scale_for(std::uint32_t size) const noexcept;
        [[nodiscard]] const entry *rasterise(std::uint32_t size, char32_t code_point);
        [[nodiscard]] bool allocate(std::uint32_t w, std::uint32_t h, std::uint32_t &x, std::uint32_t &y);
        [[nodiscard]] bool grow();

        desc desc_; // Set by the constructors; a `{}` here would need the nested defaults before the class is complete.
        std::unique_ptr<face> face_;
        std::vector<std::byte> font_bytes_; ///< The face points into this, so it lives as long as the face.
        std::string error_;

        std::vector<std::byte> atlas_;
        std::uint32_t width_ = 0;
        std::uint32_t height_ = 0;
        std::uint32_t shelf_x_ = 0; ///< The packer: a row (shelf) fills left to right, then a new row starts below.
        std::uint32_t shelf_y_ = 0;
        std::uint32_t shelf_h_ = 0;
        bool dirty_ = false;
        bool resized_ = false;
        std::uint32_t dropped_ = 0;

        std::unordered_map<cache_key, entry, cache_hash> cache_;
    };

} // namespace ui_sample
