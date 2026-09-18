/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The recursive paint pass declared in paint.hpp.
 */

#include <catalyst/ui/paint.hpp>

#include <algorithm>

namespace catalyst::ui
{

    namespace
    {
        [[nodiscard]] corners_px resolve_radii(const corners_length &radii, const resolve_context &ctx) noexcept
        {
            // Percentages resolve against the box's own width, which the caller has put in
            // parent_width_px. CSS resolves the vertical component against the height and gets an
            // ellipse; one circular radius is what the tessellator draws, so one axis it is.
            return corners_px{
                resolve_or(radii.top_left, axis::x, ctx),
                resolve_or(radii.top_right, axis::x, ctx),
                resolve_or(radii.bottom_right, axis::x, ctx),
                resolve_or(radii.bottom_left, axis::x, ctx),
            };
        }

        [[nodiscard]] bool has_border(const edges_px &b) noexcept
        {
            return b.left > 0.0f || b.top > 0.0f || b.right > 0.0f || b.bottom > 0.0f;
        }

        /**
         * @brief Opens a layer over `bounds` when `opacity` calls for one under `mode`.
         * @return True when a layer was begun and `end_layer` is owed.
         */
        [[nodiscard]] bool maybe_begin_layer(batch_builder &out, opacity_mode mode, const rect &bounds, float opacity)
        {
            if (mode != opacity_mode::group || opacity >= 1.0f)
                return false;
            out.begin_layer(bounds, opacity);
            return true;
        }

        void paint_node(const tree &t, node n, batch_builder &out, const resolve_context &base, opacity_mode mode,
                        float parent_opacity)
        {
            const layout_result &lr = t.layout_of(n);
            if (!lr.laid_out || lr.hidden)
                return;

            const style &s = t.style_of(n);
            if (s.display == display_mode::none)
                return;

            const float own = std::clamp(s.opacity, 0.0f, 1.0f);
            if (own <= 0.0f || parent_opacity <= 0.0f)
                return;

            // Group: the subtree paints at full alpha into a layer the renderer blends in at `own`.
            // Multiply: `own` folds into the alpha of everything below, layer-free.
            const bool layered = maybe_begin_layer(out, mode, lr.subtree_bounds, own);
            const float opacity = layered ? parent_opacity : parent_opacity * own;

            resolve_context ctx = base;
            ctx.font_px = lr.font_px;
            ctx.parent_width_px = lr.size.x();
            ctx.parent_height_px = lr.size.y();

            const rect box = lr.border_box();
            const corners_px radii = resolve_radii(s.border_radius, ctx);

            if (!s.background.is_transparent())
                out.add_rounded_rect(box, radii, s.background.with_alpha(s.background.a * opacity));

            if (!s.border_color.is_transparent() && has_border(lr.border))
                out.add_border(box, lr.border, radii, s.border_color.with_alpha(s.border_color.a * opacity));

            const bool clips = s.overflow != overflow_mode::visible;
            if (clips)
                out.push_clip(lr.padding_box());

            if (const paint_fn painter = t.painter_of(n))
            {
                const paint_context pc{t, n, lr, s, opacity, ctx};
                painter(pc, out, t.painter_user(n));
            }

            for (const node child : t.children_of(n))
                paint_node(t, child, out, base, mode, opacity);

            if (clips)
                out.pop_clip();
            if (layered)
                out.end_layer();
        }
    } // namespace

    void paint(const tree &t, node root, batch_builder &out, const paint_params &params) noexcept
    {
        if (!t.is_valid(root))
            return;

        const float root_opacity = std::clamp(params.opacity, 0.0f, 1.0f);
        if (root_opacity <= 0.0f)
            return;

        // The root opacity behaves like an opacity on the root node: a layer over the whole tree
        // in group mode, a multiplier otherwise.
        const layout_result &lr = t.layout_of(root);
        const bool layered = lr.laid_out && maybe_begin_layer(out, params.compositing, lr.subtree_bounds, root_opacity);
        paint_node(t, root, out, params.context, params.compositing, layered ? 1.0f : root_opacity);
        if (layered)
            out.end_layer();
    }

} // namespace catalyst::ui
