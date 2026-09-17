/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Point to node: which node is under a pixel.
 */

#pragma once

#include <catalyst/ui/geometry.hpp>
#include <catalyst/ui/node.hpp>

namespace catalyst::ui
{

    /**
     * @brief Finds the deepest, topmost node under a point.
     * @details Walks the subtree in reverse paint order, so the last-painted child wins where
     * siblings overlap, and returns the first node whose border box contains the point and whose
     * `pointer_events` is not `none`. A node whose `overflow` clips does not let the point reach
     * children outside its padding box, since those children are not visible there. Nodes that
     * layout hid, or that were never laid out, are skipped with their subtrees. Children that
     * overflow a `visible` parent are hittable outside it, as they are painted there.
     * @param t The tree.
     * @param root The node to start from. Its ancestors' clips are not applied.
     * @param p The point in pixels, in the same space as the layout results.
     * @return The hit node, or `null_node` when nothing under the point accepts hits.
     */
    [[nodiscard]] node hit_test(const tree &t, node root, const point &p) noexcept;

} // namespace catalyst::ui
