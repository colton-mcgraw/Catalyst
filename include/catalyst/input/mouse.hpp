/**
 * @file mouse.hpp
 * @brief Header for mouse input events and related types in the Catalyst Input module. This header defines the various event structures and enumerations related to mouse input, such as mouse movement, button presses, and wheel scrolling. These types are used within the Catalyst event system to represent and handle mouse input events in a consistent and efficient manner across different platforms. By including this header, developers can easily work with mouse input events in their applications using the Catalyst framework.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include <cstdint>

#include <catalyst/math/vec.hpp>
#include <catalyst/core/event.hpp>

/**
 * @namespace catalyst::input
 * @brief The catalyst::input namespace contains definitions and utilities related to input handling in the Catalyst framework, including keyboard input, mouse input, gamepad input, and other input devices. This namespace provides a consistent interface for handling various types of input across different platforms and input systems, allowing developers to easily manage user interactions in their applications. The key_code enumeration, which represents physical keys on a keyboard, is defined within this namespace, along with functions for converting key codes to USB HID usage IDs and handling text input events separately from physical key presses.
 * @details The catalyst::input namespace is designed to provide a comprehensive set of tools and definitions for handling user input in a consistent and efficient manner. By organizing all input-related functionality within this namespace, we can avoid naming conflicts and provide a clear structure for developers to access the various input handling utilities they need for their projects. The key_code enumeration allows for consistent representation of physical keys on a keyboard, while the to_usb_hid function provides a convenient way to convert key codes to their corresponding USB HID usage IDs. Text entry is handled separately through character or text input events, allowing for a clear distinction between physical key presses and the resulting text input, which can be affected by factors such as keyboard layout and modifier keys. This design allows for flexibility in handling different types of input while maintaining a clear and organized structure for developers to work with.
 */
namespace catalyst::input {
    /**
     * @enum mouse_button
     * @brief An enumeration representing the different mouse buttons that can be interacted with. This includes the left, right, and middle buttons, as well as additional buttons (x1 and x2) that may be present on some mice. The unknown value is used to represent an unspecified or unrecognized button. This enumeration is used in mouse button events to indicate which button was involved in the event.
     * @details The mouse_button enumeration provides a clear and consistent way to represent the various buttons on a mouse device. By using this enumeration in mouse button events, developers can easily determine which button was pressed or released, allowing for responsive and intuitive handling of mouse input in their applications.
     */
    enum class mouse_button : std::uint8_t {
        left,
        right,
        middle,
        x1,
        x2,
        unknown
    };
    /**
     * @enum mouse_button_action
     * @brief An enumeration representing the different actions that can occur with mouse buttons, such as pressing, releasing, or double-clicking a button. This enumeration is used in mouse button events to indicate the specific action that occurred with a mouse button interaction, allowing for proper handling of different types of mouse button interactions in applications. By distinguishing between button actions, developers can implement features such as double-click behavior and differentiate between initial button presses and releases, enabling more responsive and intuitive input handling.
     * @details The mouse_button_action enumeration includes values for press, release, and double_click actions. The press action indicates that a mouse button has been pressed down, the release action indicates that a mouse button has been released, and the double_click action indicates that a mouse button was clicked twice in quick succession. By using this enumeration in mouse button events, developers can implement features such as double-click behavior and differentiate between initial button presses and releases, allowing for more responsive and intuitive input handling in applications.
     */
    enum class mouse_button_action : std::uint8_t {
        press,
        release,
        double_click
    };
    /**
     * @struct mouse_move_event
     * @brief A structure representing a mouse movement event, which includes information about the current position of the mouse cursor in pixels and the change in position (delta) since the last event. This structure is used in mouse movement events to provide detailed information about the specific movement of the mouse, allowing for proper handling of mouse input in applications. By including both the current position and the delta, developers can implement features such as smooth cursor movement, drag-and-drop interactions, and other mouse-related functionality that relies on tracking the movement of the cursor.
     * @details The mouse_move_event structure inherits from core::event<mouse_move_event>, allowing it to be used as an event type within the Catalyst event system. It contains a position_px field that represents the current position of the mouse cursor in pixels, and a delta_px field that represents the change in position since the last event. This structure provides comprehensive information about a mouse movement interaction, enabling developers to implement responsive and intuitive handling of mouse input in their applications.
     */
    struct mouse_move_event : public core::event<mouse_move_event> {
        /**
         * @var position_px
         * @brief The current position of the mouse cursor in pixels. This field represents the absolute position of the mouse cursor on the screen or within a specific window, depending on the context of the event. By providing the position in pixels, developers can easily work with the cursor's location for various purposes such as UI interactions, drawing operations, or other features that require knowledge of the cursor's position.
         */
        math::vec2<std::int32_t> position_px{};
        /**
         * @var delta_px
         * @brief The change in position of the mouse cursor in pixels since the last event. This field represents the relative movement of the mouse cursor, indicating how much the cursor has moved in both the x and y directions since the last mouse movement event. By providing the delta in pixels, developers can easily implement features such as smooth cursor movement, drag-and-drop interactions, and other mouse-related functionality that relies on tracking the movement of the cursor.
         */
        math::vec2<std::int32_t> delta_px{};
    };
    /**
     * @struct mouse_button_event
     * @brief A structure representing a mouse button event, which includes information about the specific button involved, the action that occurred (press, release, or double-click), and the position of the mouse cursor at the time of the event. This structure is used in mouse button events to provide detailed information about specific interactions with mouse buttons, allowing for proper handling of mouse input in applications. By including the button, action, and position information, developers can implement features such as context menus on right-click, drag-and-drop interactions with the left button, and other mouse-related functionality that relies on tracking button interactions and cursor position.
     * @details The mouse_button_event structure inherits from core::event<mouse_button_event>, allowing it to be used as an event type within the Catalyst event system. It contains a button field that represents which mouse button was involved in the event, an action field that indicates whether the button was pressed, released, or double-clicked, and a position_px field that represents the position of the mouse cursor at the time of the event. This structure provides comprehensive information about a mouse button interaction, enabling developers to implement responsive and intuitive handling of mouse input in their applications.
     */
    struct mouse_button_event : public core::event<mouse_button_event> {
        /**
         * @var button
         * @brief The specific mouse button involved in the event. This field represents which mouse button was interacted with, such as left, right, middle, x1, or x2. By providing this information, developers can implement features that depend on specific button interactions, such as context menus on right-click or drag-and-drop with the left button.
         */
        mouse_button button{ mouse_button::unknown };
        /**
         * @var action
         * @brief The specific action that occurred with the mouse button interaction. This field indicates whether the button was pressed, released, or double-clicked, allowing for proper handling of different types of mouse button interactions in applications. By distinguishing between button actions, developers can implement features such as double-click behavior and differentiate between initial button presses and releases, enabling more responsive and intuitive input handling.
         */
        mouse_button_action action{ mouse_button_action::press };
        /**
         * @var position_px
         * @brief The position of the mouse cursor in pixels at the time of the event. This field represents the absolute position of the mouse cursor on the screen or within a specific window when the button interaction occurred. By providing the position in pixels, developers can easily work with the cursor's location for various purposes such as UI interactions, drawing operations, or other features that require knowledge of the cursor's position during button interactions.
         */
        math::vec2<std::int32_t> position_px{};
    };
    /**
     * @struct mouse_wheel_event
     * @brief A structure representing a mouse wheel event, which includes information about the position of the mouse cursor in pixels and the amount of wheel movement (delta) since the last event. This structure is used in mouse wheel events to provide detailed information about the specific movement of the mouse wheel, allowing for proper handling of mouse input in applications. By including both the current position and the delta, developers can implement features such as zooming, scrolling, and other mouse-related functionality that relies on tracking the movement of the mouse wheel.
     * @details The mouse_wheel_event structure inherits from core::event<mouse_wheel_event>, allowing it to be used as an event type within the Catalyst event system. It contains a position_px field that represents the current position of the mouse cursor in pixels, and a delta field that represents the change in wheel movement since the last event. The delta is typically expressed in "ticks" (e.g. Win32 WHEEL_DELTA = 120) and should be normalized by the platform layer if line/pixel scrolling semantics are desired. This structure provides comprehensive information about a mouse wheel interaction, enabling developers to implement responsive and intuitive handling of mouse input in their applications.
     */
    struct mouse_wheel_event : public core::event<mouse_wheel_event> {
        /**
         * @var position_px
         * @brief The current position of the mouse cursor in pixels. This field represents the absolute position of the mouse cursor on the screen or within a specific window, depending on the context of the event. By providing the position in pixels, developers can easily work with the cursor's location for various purposes such as UI interactions, drawing operations, or other features that require knowledge of the cursor's position.
         */
        math::vec2<std::int32_t> position_px{};
        /**
         * @var delta
         * @brief The change in wheel movement since the last event. This field represents the amount of movement of the mouse wheel, typically expressed in "ticks" (e.g. Win32 WHEEL_DELTA = 120). The delta should be normalized by the platform layer if line/pixel scrolling semantics are desired. By providing the delta, developers can implement features such as zooming, scrolling, and other mouse-related functionality that relies on tracking the movement of the mouse wheel.
         */
        math::vec2<float> delta{};
    };

} // namespace catalyst::input