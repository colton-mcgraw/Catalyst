/**
 * @file mouse.hpp
 * @brief Mouse input events and related types for the Catalyst Input module: button and button-set enumerations, and the
 * move, button, wheel, enter/leave and raw-motion events that the platform layer publishes for a window.
 * @details Positions are in client-area pixels of the window named by the event. Every event carries the
 * platform::window_id (as a plain integer, so the input module does not depend on the platform module) of the window that
 * generated it and the keyboard modifier state sampled at that moment, which is what UI code needs for shift-click and
 * control-drag style interactions.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <catalyst/core/event.hpp>
#include <catalyst/input/keyboard.hpp>
#include <catalyst/math/vec.hpp>

namespace catalyst::input
{
    /**
     * @enum mouse_button
     * @brief Identifies a single mouse button. x1 and x2 are the "back"/"forward" thumb buttons found on many mice.
     */
    enum class mouse_button : std::uint8_t
    {
        left,
        right,
        middle,
        x1,
        x2,
        unknown
    };

    /** @brief Number of distinct buttons in mouse_button (excluding unknown). */
    inline constexpr std::size_t mouse_button_count = 5;

    /**
     * @enum mouse_buttons
     * @brief Bit set of mouse buttons, used to report which buttons are held during a move or drag.
     */
    enum class mouse_buttons : std::uint8_t
    {
        none = 0x00,
        left = 0x01,
        right = 0x02,
        middle = 0x04,
        x1 = 0x08,
        x2 = 0x10
    };

    [[nodiscard]] inline constexpr mouse_buttons operator|(mouse_buttons a, mouse_buttons b) noexcept
    {
        using u = std::underlying_type_t<mouse_buttons>;
        return static_cast<mouse_buttons>(static_cast<u>(a) | static_cast<u>(b));
    }
    [[nodiscard]] inline constexpr mouse_buttons operator&(mouse_buttons a, mouse_buttons b) noexcept
    {
        using u = std::underlying_type_t<mouse_buttons>;
        return static_cast<mouse_buttons>(static_cast<u>(a) & static_cast<u>(b));
    }
    [[nodiscard]] inline constexpr mouse_buttons operator~(mouse_buttons a) noexcept
    {
        using u = std::underlying_type_t<mouse_buttons>;
        return static_cast<mouse_buttons>(static_cast<u>(~static_cast<u>(a) & 0x1Fu));
    }
    inline constexpr mouse_buttons &operator|=(mouse_buttons &a, mouse_buttons b) noexcept { return a = (a | b); }
    inline constexpr mouse_buttons &operator&=(mouse_buttons &a, mouse_buttons b) noexcept { return a = (a & b); }

    /** @brief Converts a single button into its bit-set representation (unknown maps to none). */
    [[nodiscard]] inline constexpr mouse_buttons to_mouse_buttons(mouse_button b) noexcept
    {
        if (b == mouse_button::unknown)
            return mouse_buttons::none;
        return static_cast<mouse_buttons>(1u << static_cast<std::uint8_t>(b));
    }

    /** @brief True if @p set contains @p b. */
    [[nodiscard]] inline constexpr bool has_button(mouse_buttons set, mouse_button b) noexcept
    {
        return (set & to_mouse_buttons(b)) != mouse_buttons::none;
    }

    /**
     * @enum mouse_button_action
     * @brief Whether a button went down or came up. Double-clicks are reported as a press with mouse_button_event::clicks
     * equal to 2, so every press is still paired with exactly one release.
     */
    enum class mouse_button_action : std::uint8_t
    {
        press,
        release
    };

    /**
     * @struct mouse_move_event
     * @brief The cursor moved inside (or, while a button is held, outside) the window's client area.
     */
    struct mouse_move_event : public core::event<mouse_move_event>
    {
        /** @brief The platform::window_id of the window the event belongs to. */
        std::uint64_t window{0};
        /** @brief Cursor position in client-area pixels. May lie outside the client area while a button is held. */
        math::vec2<std::int32_t> position_px{};
        /** @brief Movement since the previous move event for this window, in pixels. Zero on the first event. */
        math::vec2<std::int32_t> delta_px{};
        /** @brief The buttons that were held while the cursor moved. */
        mouse_buttons buttons{mouse_buttons::none};
        /** @brief Keyboard modifier state at the time of the event. */
        key_modifiers modifiers{key_modifiers::none};
    };

    /**
     * @struct mouse_button_event
     * @brief A mouse button was pressed or released over the window (or while the window held the mouse capture).
     */
    struct mouse_button_event : public core::event<mouse_button_event>
    {
        /** @brief The platform::window_id of the window the event belongs to. */
        std::uint64_t window{0};
        /** @brief The button involved. */
        mouse_button button{mouse_button::unknown};
        /** @brief Press or release. */
        mouse_button_action action{mouse_button_action::press};
        /**
         * @brief For presses, the number of consecutive clicks this press completes (1 = single, 2 = double-click).
         * Always 1 for releases.
         */
        std::uint8_t clicks{1};
        /** @brief Cursor position in client-area pixels. */
        math::vec2<std::int32_t> position_px{};
        /** @brief Keyboard modifier state at the time of the event. */
        key_modifiers modifiers{key_modifiers::none};
    };

    /**
     * @struct mouse_wheel_event
     * @brief The wheel (or a touchpad scroll gesture) moved.
     */
    struct mouse_wheel_event : public core::event<mouse_wheel_event>
    {
        /** @brief The platform::window_id of the window the event belongs to. */
        std::uint64_t window{0};
        /** @brief Cursor position in client-area pixels. */
        math::vec2<std::int32_t> position_px{};
        /**
         * @brief Scroll amount in wheel "notches": +y scrolls away from the user (up), +x scrolls right. High-resolution
         * devices report fractional values. Multiply by the application's lines-per-notch setting for line scrolling.
         */
        math::vec2<float> delta{};
        /** @brief Keyboard modifier state at the time of the event. */
        key_modifiers modifiers{key_modifiers::none};
    };

    /**
     * @struct mouse_enter_event
     * @brief The cursor entered the window's client area. Always followed eventually by a mouse_leave_event.
     */
    struct mouse_enter_event : public core::event<mouse_enter_event>
    {
        /** @brief The platform::window_id of the window the cursor entered. */
        std::uint64_t window{0};
        /** @brief Cursor position in client-area pixels. */
        math::vec2<std::int32_t> position_px{};
    };

    /**
     * @struct mouse_leave_event
     * @brief The cursor left the window's client area.
     */
    struct mouse_leave_event : public core::event<mouse_leave_event>
    {
        /** @brief The platform::window_id of the window the cursor left. */
        std::uint64_t window{0};
    };

    /**
     * @struct mouse_raw_move_event
     * @brief Unaccelerated, unclipped relative mouse motion straight from the device. Published only while the window's
     * cursor mode is platform::cursor_mode::captured (see platform::set_cursor_mode); this is the event to drive a
     * first-person camera from, because it keeps reporting motion after the cursor has been pinned to the window edge.
     */
    struct mouse_raw_move_event : public core::event<mouse_raw_move_event>
    {
        /** @brief The platform::window_id of the window that has captured the cursor. */
        std::uint64_t window{0};
        /** @brief Motion in device counts (not pixels): sign matches screen axes, +y is towards the user. */
        math::vec2<std::int32_t> delta{};
    };

} // namespace catalyst::input
