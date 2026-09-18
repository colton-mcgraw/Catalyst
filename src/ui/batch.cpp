/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Shape tessellation and command merging for the UI draw list.
 */

#include <catalyst/ui/batch.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace catalyst::ui
{

    namespace
    {
        constexpr float k_infinity = std::numeric_limits<float>::infinity();

        [[nodiscard]] rect intersect(const rect &a, const rect &b) noexcept
        {
            return rect::from_min_max(point{std::max(a.min.x(), b.min.x()), std::max(a.min.y(), b.min.y())},
                                      point{std::min(a.max.x(), b.max.x()), std::min(a.max.y(), b.max.y())});
        }

        [[nodiscard]] float max_radius(const corners_px &r) noexcept
        {
            return std::max({r.top_left, r.top_right, r.bottom_right, r.bottom_left, 0.0f});
        }

        /**
         * @brief Scales the radii down so that no two on one side sum to more than that side.
         * @details One factor for all four, as CSS specifies: scaling corners independently would
         * turn a circle into a shape with a visible seam where two differently scaled arcs meet.
         */
        [[nodiscard]] corners_px clamp_radii(const rect &r, corners_px radii) noexcept
        {
            radii.top_left = std::max(radii.top_left, 0.0f);
            radii.top_right = std::max(radii.top_right, 0.0f);
            radii.bottom_right = std::max(radii.bottom_right, 0.0f);
            radii.bottom_left = std::max(radii.bottom_left, 0.0f);

            const float w = r.width();
            const float h = r.height();
            float factor = 1.0f;
            const auto limit = [&factor](float sum, float side)
            {
                if (sum > side && sum > 0.0f)
                    factor = std::min(factor, side / sum);
            };
            limit(radii.top_left + radii.top_right, w);
            limit(radii.bottom_left + radii.bottom_right, w);
            limit(radii.top_left + radii.bottom_left, h);
            limit(radii.top_right + radii.bottom_right, h);

            if (factor < 1.0f)
            {
                radii.top_left *= factor;
                radii.top_right *= factor;
                radii.bottom_right *= factor;
                radii.bottom_left *= factor;
            }
            return radii;
        }

        [[nodiscard]] bool is_square(const corners_px &r) noexcept
        {
            return max_radius(r) <= 0.0f;
        }
    } // namespace

    void render_batch::clear() noexcept
    {
        vertices.clear();
        indices.clear();
        commands.clear();
        layers.clear();
    }

    std::uint32_t corner_segments(float radius) noexcept
    {
        if (!(radius > 0.0f))
            return 1u;
        // Chord error for n segments over a quarter circle is r * (1 - cos(pi / 4n)). Solving for
        // half a pixel gives roughly sqrt(r) segments; the constant is tuned so a 4 px radius gets
        // 2 and a 100 px radius gets 15, and the clamp keeps a giant radius from costing more than
        // a rounded rectangle is worth.
        const float n = std::ceil(1.5f * std::sqrt(radius));
        return static_cast<std::uint32_t>(std::clamp(n, 1.0f, 32.0f));
    }

    batch_builder::batch_builder(render_batch &batch, const rect &clip) noexcept : batch_(batch)
    {
        clip_stack_.push_back(clip);
    }

    rect batch_builder::unbounded_clip() noexcept
    {
        return rect::from_min_max(point{-k_infinity, -k_infinity}, point{k_infinity, k_infinity});
    }

    void batch_builder::push_clip(const rect &r)
    {
        clip_stack_.push_back(intersect(clip_stack_.back(), r));
    }

    void batch_builder::pop_clip() noexcept
    {
        // The bottom entry is the builder's own clip and stays; a mismatched pop is a caller bug
        // that should not turn into an empty stack and a crash on the next shape.
        if (clip_stack_.size() > 1u)
            clip_stack_.pop_back();
    }

    layer_id batch_builder::begin_layer(const rect &bounds, float opacity)
    {
        // The layer's rectangle is also its clip: nothing painted outside it can reach the target,
        // so culling against it here is free and matches what the renderer's scissor would do.
        push_clip(bounds);

        const layer_id id = static_cast<layer_id>(batch_.layers.size());
        batch_.layers.push_back(ui::layer{ // Qualified: the member function layer() shadows the struct here.
            .bounds = clip_stack_.back(),
            .opacity = std::clamp(opacity, 0.0f, 1.0f),
            .parent = layer(),
            .first_command = static_cast<std::uint32_t>(batch_.commands.size()),
            .end_command = static_cast<std::uint32_t>(batch_.commands.size()),
        });
        layer_stack_.push_back(id);
        return id;
    }

    void batch_builder::end_layer() noexcept
    {
        if (layer_stack_.empty())
            return;
        batch_.layers[layer_stack_.back()].end_command = static_cast<std::uint32_t>(batch_.commands.size());
        layer_stack_.pop_back();
        pop_clip();
    }

    draw_command &batch_builder::current_command()
    {
        const rect &clip = clip_stack_.back();
        const layer_id current_layer = layer();
        if (!batch_.commands.empty())
        {
            draw_command &last = batch_.commands.back();
            const bool contiguous = last.first_index + last.index_count == batch_.indices.size();
            if (contiguous && last.texture == texture_ && last.layer == current_layer && last.clip == clip)
                return last;
        }
        batch_.commands.push_back(
            draw_command{static_cast<std::uint32_t>(batch_.indices.size()), 0u, clip, texture_, current_layer});
        return batch_.commands.back();
    }

    bool batch_builder::culled(const rect &bounds) const noexcept
    {
        const rect &clip = clip_stack_.back();
        return clip.is_empty() || bounds.is_empty() || !bounds.intersects(clip);
    }

    index batch_builder::push_vertex(const point &p, const point &uv, std::uint32_t packed)
    {
        batch_.vertices.push_back(vertex{p, uv, packed});
        return static_cast<index>(batch_.vertices.size() - 1u);
    }

    void batch_builder::push_triangle(draw_command &cmd, index a, index b, index c)
    {
        batch_.indices.push_back(a);
        batch_.indices.push_back(b);
        batch_.indices.push_back(c);
        cmd.index_count += 3u;
    }

    void batch_builder::add_rect(const rect &r, const color &c)
    {
        add_rect(r, rect::from_min_max(point{0.0f, 0.0f}, point{0.0f, 0.0f}), c);
    }

    void batch_builder::add_rect(const rect &r, const rect &uv, const color &c)
    {
        if (c.is_transparent() || culled(r))
            return;

        draw_command &cmd = current_command();
        const std::uint32_t packed = c.to_rgba8();
        const index tl = push_vertex(r.min, uv.min, packed);
        const index tr = push_vertex(point{r.max.x(), r.min.y()}, point{uv.max.x(), uv.min.y()}, packed);
        const index br = push_vertex(r.max, uv.max, packed);
        const index bl = push_vertex(point{r.min.x(), r.max.y()}, point{uv.min.x(), uv.max.y()}, packed);
        push_triangle(cmd, tl, tr, br);
        push_triangle(cmd, tl, br, bl);
    }

    void batch_builder::append_outline(const rect &r, const corners_px &radii, std::uint32_t points_per_corner)
    {
        constexpr float pi = std::numbers::pi_v<float>;
        constexpr float half_pi = pi / 2.0f;

        // Each corner's arc centre, radius and start angle. With y down, the angle runs clockwise on
        // screen, so starting the top-left arc at pi (leftmost point) and sweeping a quarter turn
        // ends at the topmost point, and the four corners in this order trace the outline
        // clockwise without a seam.
        struct corner
        {
            point center;
            float radius;
            float start;
        };
        const corner corners[4] = {
            {point{r.min.x() + radii.top_left, r.min.y() + radii.top_left}, radii.top_left, pi},
            {point{r.max.x() - radii.top_right, r.min.y() + radii.top_right}, radii.top_right, 3.0f * half_pi},
            {point{r.max.x() - radii.bottom_right, r.max.y() - radii.bottom_right}, radii.bottom_right, 0.0f},
            {point{r.min.x() + radii.bottom_left, r.max.y() - radii.bottom_left}, radii.bottom_left, half_pi},
        };

        // One point per corner collapses the arc to the corner itself, which is exactly right for a
        // square shape and is why a plain border costs eight vertices rather than eight duplicates.
        const float divisor = (points_per_corner > 1u) ? static_cast<float>(points_per_corner - 1u) : 1.0f;

        for (const corner &k : corners)
        {
            for (std::uint32_t i = 0; i < points_per_corner; ++i)
            {
                const float angle = k.start + half_pi * static_cast<float>(i) / divisor;
                outline_.push_back(
                    point{k.center.x() + std::cos(angle) * k.radius, k.center.y() + std::sin(angle) * k.radius});
            }
        }
    }

    void batch_builder::add_rounded_rect(const rect &r, const corners_px &radii_in, const color &c)
    {
        if (c.is_transparent() || culled(r))
            return;

        const corners_px radii = clamp_radii(r, radii_in);
        if (is_square(radii))
        {
            add_rect(r, c);
            return;
        }

        outline_.clear();
        append_outline(r, radii, corner_segments(max_radius(radii)) + 1u);

        draw_command &cmd = current_command();
        const std::uint32_t packed = c.to_rgba8();
        const point uv{};
        const index center = push_vertex(r.center(), uv, packed);
        const index first = push_vertex(outline_.front(), uv, packed);
        index previous = first;
        for (std::size_t i = 1; i < outline_.size(); ++i)
        {
            const index next = push_vertex(outline_[i], uv, packed);
            push_triangle(cmd, center, previous, next);
            previous = next;
        }
        push_triangle(cmd, center, previous, first);
    }

    void batch_builder::add_border(const rect &r, const edges_px &widths, const corners_px &radii_in, const color &c)
    {
        if (c.is_transparent() || culled(r))
            return;
        if (widths.left <= 0.0f && widths.top <= 0.0f && widths.right <= 0.0f && widths.bottom <= 0.0f)
            return;

        const corners_px outer = clamp_radii(r, radii_in);
        const rect inner_rect = deflate(r, widths);
        if (inner_rect.is_empty())
        {
            add_rounded_rect(r, outer, c);
            return;
        }

        const corners_px inner{
            std::max(outer.top_left - std::max(widths.top, widths.left), 0.0f),
            std::max(outer.top_right - std::max(widths.top, widths.right), 0.0f),
            std::max(outer.bottom_right - std::max(widths.bottom, widths.right), 0.0f),
            std::max(outer.bottom_left - std::max(widths.bottom, widths.left), 0.0f),
        };

        // One point count for both rings, decided by the outer radii, so the outlines pair up one to
        // one. An inner corner whose radius the border width ate down to zero still emits its full
        // share of points, coincident at the corner, which is the arc degenerating correctly rather
        // than a ring whose vertices no longer line up with its partner's.
        const std::uint32_t points = is_square(outer) ? 1u : corner_segments(max_radius(outer)) + 1u;
        outline_.clear();
        append_outline(r, outer, points);
        const std::size_t ring = outline_.size();
        append_outline(inner_rect, clamp_radii(inner_rect, inner), points);

        draw_command &cmd = current_command();
        const std::uint32_t packed = c.to_rgba8();
        const point uv{};
        const index base = static_cast<index>(batch_.vertices.size());
        for (const point &p : outline_)
            (void)push_vertex(p, uv, packed);

        for (std::size_t i = 0; i < ring; ++i)
        {
            const std::size_t j = (i + 1u) % ring;
            const index o0 = base + static_cast<index>(i);
            const index o1 = base + static_cast<index>(j);
            const index i0 = base + static_cast<index>(ring + i);
            const index i1 = base + static_cast<index>(ring + j);
            push_triangle(cmd, o0, o1, i1);
            push_triangle(cmd, o0, i1, i0);
        }
    }

    void batch_builder::add_line(const point &a, const point &b, float thickness, const color &c)
    {
        if (c.is_transparent() || !(thickness > 0.0f))
            return;

        const point d = b - a;
        const float length = std::sqrt(d.x() * d.x() + d.y() * d.y());
        if (!(length > 0.0f))
            return;

        // Perpendicular offset of half the thickness on each side.
        const point n{-d.y() / length * thickness * 0.5f, d.x() / length * thickness * 0.5f};
        const point p0 = a + n;
        const point p1 = b + n;
        const point p2 = b - n;
        const point p3 = a - n;

        rect bounds = rect::from_min_max(p0, p0);
        for (const point &p : {p1, p2, p3})
        {
            bounds.min = point{std::min(bounds.min.x(), p.x()), std::min(bounds.min.y(), p.y())};
            bounds.max = point{std::max(bounds.max.x(), p.x()), std::max(bounds.max.y(), p.y())};
        }
        // A perfectly horizontal or vertical hairline has zero-area bounds; pad so it is not culled.
        bounds = inflate(bounds, edges_px::all(0.5f));
        if (culled(bounds))
            return;

        draw_command &cmd = current_command();
        const std::uint32_t packed = c.to_rgba8();
        const point uv{};
        const index v0 = push_vertex(p0, uv, packed);
        const index v1 = push_vertex(p1, uv, packed);
        const index v2 = push_vertex(p2, uv, packed);
        const index v3 = push_vertex(p3, uv, packed);
        push_triangle(cmd, v0, v1, v2);
        push_triangle(cmd, v0, v2, v3);
    }

    void batch_builder::add_triangles(std::span<const vertex> vertices, std::span<const index> indices)
    {
        if (vertices.empty() || indices.size() < 3u)
            return;

        rect bounds = rect::from_min_max(vertices.front().position, vertices.front().position);
        for (const vertex &v : vertices)
        {
            bounds.min = point{std::min(bounds.min.x(), v.position.x()), std::min(bounds.min.y(), v.position.y())};
            bounds.max = point{std::max(bounds.max.x(), v.position.x()), std::max(bounds.max.y(), v.position.y())};
        }
        bounds = inflate(bounds, edges_px::all(0.5f));
        if (culled(bounds))
            return;

        draw_command &cmd = current_command();
        const index base = static_cast<index>(batch_.vertices.size());
        batch_.vertices.insert(batch_.vertices.end(), vertices.begin(), vertices.end());

        const std::size_t triangles = indices.size() / 3u;
        for (std::size_t t = 0; t < triangles; ++t)
            push_triangle(cmd, base + indices[t * 3u], base + indices[t * 3u + 1u], base + indices[t * 3u + 2u]);
    }

} // namespace catalyst::ui
