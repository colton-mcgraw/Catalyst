/*
 * @file font_provider.cpp
 * @brief Implements `ui_sample::ttf_text_provider` over stb_truetype.
 * @details Rasterisation goes through `stbtt_MakeCodepointBitmap` straight into the atlas at the packer's position,
 * so a glyph is never copied twice. Sizes map an em to `size_px` pixels (`stbtt_ScaleForMappingEmToPixels`), which is
 * what a CSS-style `font_size` means and what `null_text_provider` assumed when it took ascent as 0.8 em. Without
 * stb (`CATALYST_UI_SAMPLE_HAS_STB=0`) `open` fails with a message and the sample falls back to the null provider.
 * License: MIT (see LICENSE).
 */

#include "font_provider.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <utility>

#if CATALYST_UI_SAMPLE_HAS_STB
#include <stb_truetype.h>
#endif

namespace ui_sample
{
    namespace ui = catalyst::ui;

    struct ttf_text_provider::face
    {
#if CATALYST_UI_SAMPLE_HAS_STB
        stbtt_fontinfo info{};
        int ascent = 0;   ///< Font units; scaled per size.
        int descent = 0;  ///< Negative in stb's convention.
        int line_gap = 0;
#endif
    };

    ttf_text_provider::ttf_text_provider() : ttf_text_provider(desc{}) {}

    ttf_text_provider::ttf_text_provider(const desc &d) : desc_(d)
    {
        desc_.initial_size = std::max<std::uint32_t>(desc_.initial_size, 16);
        desc_.max_size = std::max(desc_.max_size, desc_.initial_size);
        if (desc_.key == ui::no_texture)
            desc_.key = 1;
        width_ = height_ = desc_.initial_size;
        atlas_.assign(static_cast<std::size_t>(width_) * height_, std::byte{0});
    }

    ttf_text_provider::~ttf_text_provider() = default;

    bool ttf_text_provider::open(const std::filesystem::path &file, int index)
    {
        std::ifstream in(file, std::ios::binary);
        if (!in)
        {
            error_ = "cannot open " + file.string();
            return false;
        }
        std::vector<char> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (raw.empty())
        {
            error_ = file.string() + " is empty";
            return false;
        }
        return open(std::as_bytes(std::span<const char>(raw)), index);
    }

    bool ttf_text_provider::open(std::span<const std::byte> bytes, int index)
    {
#if CATALYST_UI_SAMPLE_HAS_STB
        std::vector<std::byte> copy(bytes.begin(), bytes.end());
        auto f = std::make_unique<face>();

        const auto *data = reinterpret_cast<const unsigned char *>(copy.data());
        const int offset = stbtt_GetFontOffsetForIndex(data, index);
        if (offset < 0)
        {
            error_ = "no face " + std::to_string(index) + " in the file";
            return false;
        }
        if (!stbtt_InitFont(&f->info, data, offset))
        {
            error_ = "stb_truetype could not parse the face";
            return false;
        }
        stbtt_GetFontVMetrics(&f->info, &f->ascent, &f->descent, &f->line_gap);

        // Commit: the face points into `font_bytes_`, so the bytes move in first and stay put.
        font_bytes_ = std::move(copy);
        f->info.data = reinterpret_cast<unsigned char *>(font_bytes_.data());
        face_ = std::move(f);
        cache_.clear();
        std::ranges::fill(atlas_, std::byte{0});
        shelf_x_ = shelf_y_ = shelf_h_ = 0;
        dirty_ = true;
        error_.clear();
        return true;
#else
        (void)bytes;
        (void)index;
        error_ = "this build has no stb_truetype (CATALYST_RESOURCE_STB is off)";
        return false;
#endif
    }

    // ---- text_provider --------------------------------------------------------------------------

    std::uint32_t ttf_text_provider::quantise(float size_px) noexcept
    {
        if (!(size_px > 0.0f))
            return 1;
        return static_cast<std::uint32_t>(std::lround(std::min(size_px, 512.0f)));
    }

    float ttf_text_provider::scale_for(std::uint32_t size) const noexcept
    {
#if CATALYST_UI_SAMPLE_HAS_STB
        if (face_ == nullptr)
            return 0.0f;
        return stbtt_ScaleForMappingEmToPixels(&face_->info, static_cast<float>(size));
#else
        (void)size;
        return 0.0f;
#endif
    }

    ui::font_metrics ttf_text_provider::metrics(ui::font_id, float size_px)
    {
#if CATALYST_UI_SAMPLE_HAS_STB
        if (face_ == nullptr)
            return {};
        const float scale = scale_for(quantise(size_px));
        return ui::font_metrics{
            .ascent = static_cast<float>(face_->ascent) * scale,
            .descent = -static_cast<float>(face_->descent) * scale,
            .line_gap = static_cast<float>(face_->line_gap) * scale,
        };
#else
        (void)size_px;
        return {};
#endif
    }

    bool ttf_text_provider::glyph_of(ui::font_id, float size_px, char32_t code_point, ui::glyph &out)
    {
        if (face_ == nullptr || code_point < 0x20)
            return false;

        const entry *e = rasterise(quantise(size_px), code_point);
        if (e == nullptr)
            return false;

        out.code_point = code_point;
        out.advance = e->advance;
        out.texture = desc_.key;
        if (e->w == 0 || e->h == 0)
        {
            out.bounds = ui::rect{};
            out.uv = ui::rect{};
            return true;
        }
        out.bounds = ui::rect::from_xywh(e->bounds_x, e->bounds_y, static_cast<float>(e->w), static_cast<float>(e->h));
        const float iw = 1.0f / static_cast<float>(width_);
        const float ih = 1.0f / static_cast<float>(height_);
        out.uv = ui::rect::from_min_max(ui::point{static_cast<float>(e->x) * iw, static_cast<float>(e->y) * ih},
                                        ui::point{static_cast<float>(e->x + e->w) * iw,
                                                  static_cast<float>(e->y + e->h) * ih});
        return true;
    }

    float ttf_text_provider::kerning(ui::font_id, float size_px, char32_t left, char32_t right)
    {
#if CATALYST_UI_SAMPLE_HAS_STB
        if (face_ == nullptr)
            return 0.0f;
        const std::uint32_t size = quantise(size_px);
        const int units = stbtt_GetCodepointKernAdvance(&face_->info, static_cast<int>(left), static_cast<int>(right));
        return units == 0 ? 0.0f : static_cast<float>(units) * scale_for(size);
#else
        (void)size_px;
        (void)left;
        (void)right;
        return 0.0f;
#endif
    }

    // ---- the atlas ------------------------------------------------------------------------------

    std::span<const std::byte> ttf_text_provider::pixels() const noexcept
    {
        return atlas_;
    }

    bool ttf_text_provider::take_dirty() noexcept
    {
        return std::exchange(dirty_, false);
    }

    bool ttf_text_provider::take_resized() noexcept
    {
        return std::exchange(resized_, false);
    }

    const ttf_text_provider::entry *ttf_text_provider::rasterise(std::uint32_t size, char32_t code_point)
    {
#if CATALYST_UI_SAMPLE_HAS_STB
        const cache_key key{size, code_point};
        if (const auto it = cache_.find(key); it != cache_.end())
            return &it->second;

        const int cp = static_cast<int>(code_point);
        if (stbtt_FindGlyphIndex(&face_->info, cp) == 0)
            return nullptr; // The face has no glyph: the seam says draw nothing and advance nothing.

        const float scale = scale_for(size);
        int advance = 0;
        int lsb = 0;
        stbtt_GetCodepointHMetrics(&face_->info, cp, &advance, &lsb);

        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetCodepointBitmapBox(&face_->info, cp, scale, scale, &x0, &y0, &x1, &y1);

        entry e;
        e.advance = static_cast<float>(advance) * scale;
        e.bounds_x = static_cast<float>(x0);
        e.bounds_y = static_cast<float>(y0);

        const int w = x1 - x0;
        const int h = y1 - y0;
        if (w > 0 && h > 0)
        {
            std::uint32_t x = 0, y = 0;
            if (allocate(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), x, y))
            {
                unsigned char *dst = reinterpret_cast<unsigned char *>(atlas_.data()) + (static_cast<std::size_t>(y) * width_ + x);
                stbtt_MakeCodepointBitmap(&face_->info, dst, w, h, static_cast<int>(width_), scale, scale, cp);
                e.x = x;
                e.y = y;
                e.w = static_cast<std::uint32_t>(w);
                e.h = static_cast<std::uint32_t>(h);
                dirty_ = true;
            }
            else
            {
                ++dropped_; // Advances still, so the line keeps its shape; the image is simply missing.
            }
        }

        return &cache_.emplace(key, e).first->second;
#else
        (void)size;
        (void)code_point;
        return nullptr;
#endif
    }

    bool ttf_text_provider::allocate(std::uint32_t w, std::uint32_t h, std::uint32_t &x, std::uint32_t &y)
    {
        const std::uint32_t pad = desc_.padding;
        const std::uint32_t need_w = w + pad;
        const std::uint32_t need_h = h + pad;

        for (;;)
        {
            // Fits on the current shelf?
            if (shelf_x_ + need_w <= width_ && shelf_y_ + need_h <= height_)
            {
                x = shelf_x_;
                y = shelf_y_;
                shelf_x_ += need_w;
                shelf_h_ = std::max(shelf_h_, need_h);
                return true;
            }
            // Start a new shelf below if there is room for one.
            if (shelf_x_ != 0 && need_w <= width_ && shelf_y_ + shelf_h_ + need_h <= height_)
            {
                shelf_y_ += shelf_h_;
                shelf_x_ = 0;
                shelf_h_ = 0;
                continue;
            }
            if (!grow())
                return false;
        }
    }

    bool ttf_text_provider::grow()
    {
        if (width_ >= desc_.max_size && height_ >= desc_.max_size)
            return false;

        // Double the shorter side; existing rows keep their pixel positions, so nothing is re-rasterised. Only the
        // texture coordinates change, which `take_resized` reports.
        const std::uint32_t new_w = (width_ <= height_ && width_ < desc_.max_size) ? width_ * 2 : width_;
        const std::uint32_t new_h = (new_w == width_) ? height_ * 2 : height_;

        std::vector<std::byte> bigger(static_cast<std::size_t>(new_w) * new_h, std::byte{0});
        for (std::uint32_t row = 0; row < height_; ++row)
            std::memcpy(bigger.data() + static_cast<std::size_t>(row) * new_w,
                        atlas_.data() + static_cast<std::size_t>(row) * width_, width_);

        atlas_ = std::move(bigger);
        width_ = new_w;
        height_ = new_h;
        dirty_ = true;
        resized_ = true;
        return true;
    }

} // namespace ui_sample
