/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The events the UI module publishes on a `catalyst::events::bus`.
 */

#pragma once

#include <catalyst/events/tag.hpp>
#include <catalyst/ui/geometry.hpp>
#include <catalyst/ui/node.hpp>

#include <cstdint>

namespace catalyst::ui
{

    /**
     * @namespace catalyst::ui::tags
     * @brief Static event tags for the UI events, in the `0x0004'0000` block.
     * @details Input owns `0x0001'0000`, audio `0x0002'0000` and rendering `0x0003'0000`. Static
     * tags rather than type-index keys so a listener registered against a tag matches an event
     * published from another translation unit, and so tooling can name an event from a number.
     */
    namespace tags
    {
        inline constexpr events::event_type_t ui_base = 0x0004'0000u;

        inline constexpr events::event_type_t pointer_enter = ui_base + 0x00;
        inline constexpr events::event_type_t pointer_leave = ui_base + 0x01;
        inline constexpr events::event_type_t pointer_move = ui_base + 0x02;
        inline constexpr events::event_type_t pointer_down = ui_base + 0x03;
        inline constexpr events::event_type_t pointer_up = ui_base + 0x04;
        inline constexpr events::event_type_t click = ui_base + 0x05;
        inline constexpr events::event_type_t wheel = ui_base + 0x06;

        inline constexpr events::event_type_t key_down = ui_base + 0x10;
        inline constexpr events::event_type_t key_up = ui_base + 0x11;
        inline constexpr events::event_type_t text_input = ui_base + 0x12;

        inline constexpr events::event_type_t focus_gained = ui_base + 0x20;
        inline constexpr events::event_type_t focus_lost = ui_base + 0x21;
    } // namespace tags

    /**
     * @enum pointer_button
     * @brief The button a pointer event refers to.
     * @details The UI module's own vocabulary rather than `input::mouse_button`, so that
     * `catalyst_ui` does not link the input module and a touch or pen backend can feed it the same
     * events. The bridge from `catalyst::input` maps one to the other.
     */
    enum class pointer_button : std::uint8_t
    {
        none = 0,
        left,
        right,
        middle,
        extra1,
        extra2,
    };

    /**
     * @struct modifiers
     * @brief The modifier keys held during an event.
     */
    struct modifiers
    {
        bool shift = false;
        bool control = false;
        bool alt = false;
        bool super = false;

        [[nodiscard]] constexpr bool operator==(const modifiers &other) const noexcept = default;
    };

    /**
     * @struct ui_event
     * @brief Base of every UI event: a static tag and the node it is about.
     * @tparam Tag The event's static tag from `tags`.
     */
    template <events::event_type_t Tag>
    struct ui_event : events::tagged<Tag>
    {
        /** @brief The node the event is delivered to. `null_node` when nothing was under the pointer or focused. */
        node target = null_node;
    };

    /**
     * @struct pointer_event
     * @brief Base of the pointer events: where the pointer is, in viewport and in node coordinates.
     */
    template <events::event_type_t Tag>
    struct pointer_event : ui_event<Tag>
    {
        /** @brief The pointer position in pixels, in the layout's space. */
        point position{};
        /** @brief The position relative to the target's border box. Equal to `position` when there is no target. */
        point local{};
        /** @brief The modifier keys held. */
        modifiers mods{};
    };

    /** @brief The pointer moved onto a node it was not over before. Delivered after the previous node's leave. */
    struct pointer_enter_event : pointer_event<tags::pointer_enter>
    {
    };

    /** @brief The pointer moved off a node. */
    struct pointer_leave_event : pointer_event<tags::pointer_leave>
    {
    };

    /** @brief The pointer moved over the target. */
    struct pointer_move_event : pointer_event<tags::pointer_move>
    {
        /** @brief Movement since the previous move, in pixels. Zero on the first move. */
        point delta{};
    };

    /** @brief A button went down over the target. */
    struct pointer_down_event : pointer_event<tags::pointer_down>
    {
        pointer_button button = pointer_button::none;
    };

    /** @brief A button came up over the target, or over whatever captured the pointer. */
    struct pointer_up_event : pointer_event<tags::pointer_up>
    {
        pointer_button button = pointer_button::none;
    };

    /** @brief A button went down and came up on the same node. Delivered after the up. */
    struct click_event : pointer_event<tags::click>
    {
        pointer_button button = pointer_button::none;
    };

    /** @brief The wheel or a scroll gesture moved over the target. */
    struct wheel_event : pointer_event<tags::wheel>
    {
        /** @brief Scroll amount, in lines or pixels as the source reports it; positive y scrolls down. */
        point delta{};
    };

    /**
     * @struct key_event
     * @brief Base of the key events.
     * @details `key` is an opaque code the application chose when it fed the event in, typically
     * `input::key` cast to an integer. The UI module compares it to nothing.
     */
    template <events::event_type_t Tag>
    struct key_event : ui_event<Tag>
    {
        std::uint32_t key = 0;
        modifiers mods{};
        /** @brief True for the auto-repeat of a held key. */
        bool repeat = false;
    };

    /** @brief A key went down with the target focused. */
    struct key_down_event : key_event<tags::key_down>
    {
    };

    /** @brief A key came up with the target focused. */
    struct key_up_event : key_event<tags::key_up>
    {
    };

    /** @brief A character was typed with the target focused. */
    struct text_input_event : ui_event<tags::text_input>
    {
        char32_t code_point = 0;
    };

    /** @brief The target became the focused node. */
    struct focus_gained_event : ui_event<tags::focus_gained>
    {
        /** @brief The node that lost focus, or `null_node`. */
        node previous = null_node;
    };

    /** @brief The target stopped being the focused node. Delivered before the next node's gain. */
    struct focus_lost_event : ui_event<tags::focus_lost>
    {
        /** @brief The node that gains focus, or `null_node`. */
        node next = null_node;
    };

} // namespace catalyst::ui
