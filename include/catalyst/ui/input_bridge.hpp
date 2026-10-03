/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Feeds `catalyst::input` mouse and keyboard events into a `ui::interaction`.
 * @details `catalyst_ui` has its own pointer-button and modifier vocabulary so that it does not link
 * the input module. This is the translation between the two, as bus listeners: construct one over
 * the bus the input context publishes on and the interaction to drive, and every mouse move,
 * button, wheel, leave, key and text event reaches the tree from then on. Destroying the bridge
 * removes the listeners. It lives in `catalyst_ui_host`, which links both modules; an application
 * with another pointer source (touch, pen, a remote) writes its own dozen lines instead.
 */

#pragma once

#include <catalyst/events/bus.hpp>
#include <catalyst/events/token.hpp>
#include <catalyst/input/keyboard.hpp>
#include <catalyst/input/mouse.hpp>
#include <catalyst/input/text.hpp>
#include <catalyst/ui/events.hpp>
#include <catalyst/ui/geometry.hpp>
#include <catalyst/ui/interaction.hpp>

#include <array>

namespace catalyst::ui
{

    /** @brief The UI's name for an input-module mouse button; `none` for one it has no name for. */
    [[nodiscard]] pointer_button to_pointer_button(input::mouse_button b) noexcept;

    /** @brief The UI's modifier set for an input-module modifier mask. */
    [[nodiscard]] modifiers to_modifiers(input::key_modifiers m) noexcept;

    /**
     * @class input_bridge
     * @brief Listens on a bus for input events and forwards each one to an interaction.
     * @details Move-only. The listeners hold the interaction, not the bridge, so moving the bridge
     * is safe; the interaction must outlive it, and so must the bus while any dispatch is running.
     * Positions pass through unchanged: input reports client-area pixels and the tree is laid out
     * in the same pixels, so a UI drawn over the whole client area needs no mapping. Keys reach the
     * tree as the input module's `key_code` values cast to `std::uint32_t`.
     *
     * The listeners run at priority 0. An application that wants some events kept from the tree,
     * such as a game that owns the mouse while the cursor is captured, installs a bus middleware
     * for those event types that declines to pass them on, or does not construct a bridge.
     */
    class input_bridge
    {
    public:
        /** @brief A bridge that forwards nothing. */
        input_bridge() noexcept = default;

        /**
         * @brief Starts forwarding.
         * @param b The bus `input::context` publishes on.
         * @param target The interaction to drive. Must outlive the bridge.
         */
        input_bridge(events::bus &b, interaction &target);

        input_bridge(const input_bridge &) = delete;
        input_bridge &operator=(const input_bridge &) = delete;
        input_bridge(input_bridge &&) noexcept = default;
        input_bridge &operator=(input_bridge &&) noexcept = default;
        ~input_bridge() = default;

        /** @brief Stops forwarding. Safe to call twice. */
        void reset();

        /** @brief True while the listeners are installed. */
        [[nodiscard]] bool active() const noexcept { return tokens_[0].valid(); }

    private:
        std::array<events::scoped_token, 7> tokens_{};
    };

} // namespace catalyst::ui
