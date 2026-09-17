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

        /** @brief Opacity applied to the whole subtree, multiplied into every node's own. */
        float opacity = 1.0f;

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
         * @brief The opacity in effect, the product of every ancestor's and the node's own.
         * @details Multiply it into the alpha of everything painted. The pass has already applied
         * it to the background and border.
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
     * Opacity is applied per node by scaling alpha, which is not the same as compositing the
     * subtree at reduced opacity: overlapping children show through each other. True group
     * opacity needs an offscreen pass and is deferred with the renderer bridge.
     * @param t The tree.
     * @param root The node to start from. Its ancestors' clips are not applied.
     * @param out The builder to emit into. Its current clip bounds everything painted.
     * @param params Context and root opacity.
     */
    void paint(const tree &t, node root, batch_builder &out, const paint_params &params) noexcept;

} // namespace catalyst::ui
