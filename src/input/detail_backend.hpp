#pragma once

#include <catalyst/input/gamepad.hpp>

#include <cstddef>

// Interface every input backend (win32/, null/) implements. Keyboard and mouse events come from the platform module's
// window backend; the input backend is only responsible for devices that are not tied to a window.
namespace catalyst::input::detail
{

    const char *backend_name();

    // ----------------------
    // Gamepad backend API
    // ----------------------

    // Number of slots the backend can report. Never more than input::max_gamepads.
    [[nodiscard]] std::size_t gamepad_capacity() noexcept;

    // Reads the raw state of a slot into `out`. Axes are normalised (sticks [-1, 1] with +y up, triggers [0, 1]) but no
    // dead zone is applied; `out.connected` is set. Returns false (leaving `out` untouched) if the slot is empty.
    [[nodiscard]] bool read_gamepad(gamepad_id id, gamepad_state &out) noexcept;

    // Sets the vibration motors. Intensities are already clamped to [0, 1]. Returns false if unsupported or disconnected.
    bool set_gamepad_rumble(gamepad_id id, double low_frequency, double high_frequency) noexcept;

} // namespace catalyst::input::detail
