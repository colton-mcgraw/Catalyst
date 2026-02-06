#pragma once

#include <cstdint>

#include <catalyst/math/vec.hpp>

#include "catalyst/core/event.hpp"

namespace catalyst::input {

    enum class mouse_button : std::uint8_t {
        left,
        right,
        middle,
        x1,
        x2,
        unknown
    };

    enum class mouse_button_action : std::uint8_t {
        press,
        release,
        double_click
    };

    struct mouse_move_event : public core::event<mouse_move_event> {
        math::vec2<std::int32_t> position_px{};
        math::vec2<std::int32_t> delta_px{};
    };

    struct mouse_button_event : public core::event<mouse_button_event> {
        mouse_button button{ mouse_button::unknown };
        mouse_button_action action{ mouse_button_action::press };
        math::vec2<std::int32_t> position_px{};
    };

    // Wheel deltas are typically expressed in "ticks" (e.g. Win32 WHEEL_DELTA = 120) and should be
    // normalized by the platform layer if you want line/pixel scrolling semantics.
    struct mouse_wheel_event : public core::event<mouse_wheel_event> {
        math::vec2<std::int32_t> position_px{};
        math::vec2<float> delta{}; // e.g. { 0.0f, 1.0f } for one tick up
    };

} // namespace catalyst::input