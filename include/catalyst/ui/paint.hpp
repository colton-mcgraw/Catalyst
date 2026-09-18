/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The paint pass: walks a laid-out tree and emits its backgrounds, borders and content.
 */

#pragma once

#include <catalyst/ui/batch.hpp>
#include <catalyst/ui/measurement.hpp>
#include <catalyst/ui/node.hpp>
#include <catalyst/ui/style.hpp>

namespace catalyst::ui
{

    /**
     * @enum opacity_mode
     * @brief How a translucent node (`style::opacity` below one) is painted.
     */
    enum class opacity_mode : std::uint8_t
    {
        /**
         * @brief Group opacity, as CSS defines it: the subtree is painted at full opacity into a
         * `layer` of the batch, and the renderer blends the finished image in at the node's opacity.
         * Overlapping children do not show through each other. Costs an offscreen pass per
         * translucent node; needs a renderer that draws layers.
         */
        group = 0,
        /**
         * @brief The node's opacity is multiplied into the alpha of everything in its subtree.
         * No layers, no offscreen pass, but overlapping children show through each other.
         */
        multiply,
    };

    /**
     * @struct paint_params
     * @brief The inputs a paint pass needs beyond the tree and the builder.
     */
    struct paint_params
    {
        /**
         * @brief The measurement context the pass resolves visual lengths (border radii) against.
         * @details Its per-node fields are overwritten by the pass; set DPI and viewport, as for
         * `layout_params::context`. Pass the same context layout used so a `dp` radius and a `dp`
         * width agree.
         */
        resolve_context context{};

        /**
         * @brief Opacity applied to the whole subtree, treated like a `style::opacity` on the root:
         * a layer under `opacity_mode::group`, a multiplier under `opacity_mode::multiply`.
         */
        float opacity = 1.0f;

        /** @brief Whether a translucent node becomes a layer or scales its subtree's alpha. */
        opacity_mode compositing = opacity_mode::group;

        /**
         * @brief Builds parameters matching `layout_params::for_viewport`.
         * @param viewport The viewport size in pixels.
         * @param dpi_scale The display scale factor, where 1 is 96 DPI.
         */
        [[nodiscard]] static paint_params for_viewport(extent viewport, float dpi_scale = 1.0f) noexcept
        {
            paint_params p{};
            p.context.dpi_scale = dpi_scale;
            p.context.dpi_x = dpi_scale * 96.0f;
            p.context.dpi_y = dpi_scale * 96.0f;
            p.context.viewport_width_px = viewport.x();
            p.context.viewport_height_px = viewport.y();
            return p;
        }
    };

    /**
     * @struct paint_context
     * @brief What a `paint_fn` is told about the node it paints.
     */
    struct paint_context
    {
        /** @brief The tree the node lives in. */
        const ui::tree &tree;
        /** @brief The node being painted. */
        node n;
        /** @brief The node's layout, already absolute. Paint content into `layout.content_box()`. */
        const layout_result &layout;
        /** @brief The node's style. */
        const ui::style &style;
        /**
         * @brief The opacity to multiply into the alpha of everything painted.
         * @details Under `opacity_mode::multiply` it is the product of every ancestor's opacity and
         * the node's own. Under `opacity_mode::group` a translucent ancestor is a layer the renderer
         * composites instead, so this is one. The pass has already applied it to the background
         * and border either way.
         */
        float opacity;
        /** @brief The measurement context for this node: its font size and box size are filled in. */
        resolve_context context;
    };

    /**
     * @brief Paints a node and its subtree into a batch.
     * @details For each visible node, in tree order: the background as a rounded rectangle over the
     * border box, then the border ring, then, if `overflow` is not `visible`, a clip to the padding
     * box for everything below; then the node's `paint_fn`, if any; then its children in order;
     * then the clip is popped. Nodes that layout hid, or that were never laid out, are skipped with
     * their subtrees.
     *
     * A node whose opacity is below one is, under `opacity_mode::group`, wrapped in a
     * `batch_builder::begin_layer` over its `layout_result::subtree_bounds` and painted at full
     * opacity inside it; the renderer composites the layer. A `paint_fn` that draws outside its
     * node's subtree bounds is cut at the layer's edge. Under `opacity_mode::multiply` the opacity
     * is instead multiplied into the alpha of everything in the subtree, and overlapping children
     * show through each other. A node whose opacity is zero is skipped with its subtree either way.
     * @param t The tree.
     * @param root The node to start from. Its ancestors' clips are not applied.
     * @param out The builder to emit into. Its current clip bounds everything painted.
     * @param params Context, root opacity and how opacity composites.
     */
    void paint(const tree &t, node root, batch_builder &out, const paint_params &params) noexcept;

} // namespace catalyst::ui
