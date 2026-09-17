/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Pointer and keyboard state for a tree: hover, capture, focus, and the events they produce.
 */

#pragma once

#include <catalyst/events/bus.hpp>
#include <catalyst/ui/events.hpp>
#include <catalyst/ui/geometry.hpp>
#include <catalyst/ui/node.hpp>

#include <cstdint>

namespace catalyst::ui
{

    /**
     * @class interaction
     * @brief Turns raw pointer and key input into UI events on a bus.
     * @details The application feeds it what its input source reports -- a position, a button, a
     * key -- and it does the rest: hit-tests against the tree, tracks which node is hovered, pressed,
     * captured and focused, and publishes `pointer_enter_event`, `click_event` and the others on
     * the bus it was given. It publishes on the same bus every other module does rather than
     * calling callbacks, so a widget and an application listen the same way, and so a listener can
     * be middleware that swallows an event before the widget sees it.
     *
     * Every stored handle is re-validated on entry, so destroying the hovered or focused node
     * between two calls is safe: the handle is dropped without a leave or focus-lost event, since
     * there is nothing left to deliver one to.
     *
     * Not thread-safe; feed it from the thread that owns the tree.
     */
    class interaction
    {
    public:
        /**
         * @brief Binds to a tree and a bus. Both must outlive the interaction.
         * @param t The tree to hit-test.
         * @param b The bus to publish on.
         */
        interaction(const tree &t, events::bus &b) noexcept;

        /**
         * @brief Sets the node hit tests start from. Nothing is delivered until one is set.
         * @param root The root of the interactive subtree.
         */
        void set_root(node root) noexcept;

        /** @brief The root hit tests start from. */
        [[nodiscard]] node root() const noexcept { return root_; }

        // ---- pointer ----------------------------------------------------------------------------

        /**
         * @brief The pointer moved to `position`.
         * @details Publishes leave and enter when the node under the pointer changes, then a move on
         * the current target. While a node has captured the pointer, it is the target regardless of
         * position.
         */
        void pointer_moved(const point &position, const modifiers &mods = {});

        /**
         * @brief A button went down at `position`.
         * @details Publishes a down on the target, records it as pressed, and moves focus to it (or
         * clears focus when nothing was hit). Focus following the press is the plain rule Tier 6
         * widgets will refine with a focusable flag; it is what a text field expects today.
         */
        void pointer_pressed(pointer_button button, const point &position, const modifiers &mods = {});

        /**
         * @brief A button came up at `position`.
         * @details Publishes an up on the target and, if it is the node the same button went down
         * on, a click after it.
         */
        void pointer_released(pointer_button button, const point &position, const modifiers &mods = {});

        /** @brief The pointer left the surface entirely. Publishes a leave on the hovered node. */
        void pointer_left();

        /** @brief The wheel moved at `position`. */
        void wheel(const point &delta, const point &position, const modifiers &mods = {});

        // ---- keyboard ---------------------------------------------------------------------------

        /** @brief A key went down. Delivered to the focused node, or with a null target when nothing is focused. */
        void key_pressed(std::uint32_t key, const modifiers &mods = {}, bool repeat = false);

        /** @brief A key came up. Delivered like `key_pressed`. */
        void key_released(std::uint32_t key, const modifiers &mods = {});

        /** @brief A character was typed. Delivered to the focused node; dropped when nothing is focused. */
        void text_input(char32_t code_point);

        // ---- focus and capture ------------------------------------------------------------------

        /**
         * @brief Moves focus to a node, publishing focus-lost on the old node and focus-gained on the new.
         * @param n The node to focus, or `null_node` to clear focus. An invalid node clears focus.
         */
        void set_focus(node n);

        /**
         * @brief Routes every pointer event to `n` until `release_capture`, whatever is under the pointer.
         * @details What a slider or a drag needs: once the press lands, the pointer may leave the
         * node's box and the node still gets the moves and the release.
         */
        void capture(node n) noexcept;

        /** @brief Ends a capture. */
        void release_capture() noexcept;

        /** @brief The node under the pointer as of the last pointer call, or `null_node`. */
        [[nodiscard]] node hovered() const noexcept { return hovered_; }

        /** @brief The focused node, or `null_node`. */
        [[nodiscard]] node focused() const noexcept { return focused_; }

        /** @brief The node that captured the pointer, or `null_node`. */
        [[nodiscard]] node captured() const noexcept { return captured_; }

        /** @brief The node the current press started on, or `null_node`. */
        [[nodiscard]] node pressed() const noexcept { return pressed_; }

    private:
        /** @brief Drops stored handles the tree no longer recognises. */
        void validate() noexcept;

        /** @brief The node to deliver a pointer event at `p` to: the captured node, else the hit node. */
        [[nodiscard]] node target_at(const point &p) const noexcept;

        /** @brief `p` relative to `n`'s border box, or `p` itself for a null node. */
        [[nodiscard]] point local_of(node n, const point &p) const noexcept;

        /** @brief Publishes leave/enter as needed to make `n` the hovered node. */
        void update_hover(node n, const point &position, const modifiers &mods);

        const tree *tree_;
        events::bus *bus_;
        node root_ = null_node;
        node hovered_ = null_node;
        node focused_ = null_node;
        node captured_ = null_node;
        node pressed_ = null_node;
        pointer_button pressed_button_ = pointer_button::none;
        point last_position_{};
        bool has_position_ = false;
    };

} // namespace catalyst::ui
