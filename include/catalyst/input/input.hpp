/**
 * @file input.hpp
 * @brief Main header for the Catalyst Input module, which includes all input-related functionality such as handling of keyboard, mouse, gamepad, MIDI, and USB input. This header serves as the primary entry point for users of the Catalyst Input module, providing access to all the necessary types, functions, and utilities for working with various input devices in a consistent and efficient manner. By including this header, developers can easily integrate input handling into their applications using the Catalyst framework.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include <catalyst/input/gamepad.hpp>
#include <catalyst/input/keyboard.hpp>
#include <catalyst/input/midi.hpp>
#include <catalyst/input/mouse.hpp>
#include <catalyst/input/usb.hpp>

namespace catalyst::input {

const char* module_name();

} // namespace catalyst::input
