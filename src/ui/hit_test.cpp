/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The recursive hit test declared in hit_test.hpp.
 */

#include <catalyst/ui/hit_test.hpp>
#include <catalyst/ui/style.hpp>

namespace catalyst::ui
{

    namespace
    {
        [[nodiscard]] node hit(const tree &t, node n, const point &p) noexcept
        {
            const layout_result &lr = t.layout_of(n);
            if (!lr.laid_out || lr.hidden)
                return null_node;

            // Nothing in this subtree reaches the point: neither the node nor, given its overflow
            // clip, any descendant. This is what keeps a query from walking every node in the tree.
            if (!lr.subtree_bounds.contains(p))
                return null_node;

            const style &s = t.style_of(n);
            if (s.display == display_mode::none)
                return null_node;

            const bool self_hit = s.pointer_events != pointer_mode::none && lr.border_box().contains(p);

            // Under a clipping node, children are only reachable through the padding box; a point in
            // the border ring hits the node itself and nothing below.
            const bool children_reachable = s.overflow == overflow_mode::visible || lr.padding_box().contains(p);
            if (children_reachable)
            {
                const std::span<const node> children = t.children_of(n);
                for (std::size_t i = children.size(); i > 0; --i)
                {
                    const node found = hit(t, children[i - 1u], p);
                    if (!is_null(found))
                        return found;
                }
            }

            return self_hit ? n : null_node;
        }
    } // namespace

    node hit_test(const tree &t, node root, const point &p) noexcept
    {
        if (!t.is_valid(root))
            return null_node;
        return hit(t, root, p);
    }

} // namespace catalyst::ui
