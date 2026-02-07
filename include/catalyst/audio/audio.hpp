/**
 * @file audio.hpp
 * @brief Main header for the Catalyst Audio module, which includes all audio-related functionality such as audio engine management, audio playback, and other audio systems. This header serves as the primary entry point for users of the Catalyst Audio module, providing access to all the necessary types, functions, and utilities for working with audio in a consistent and efficient manner. By including this header, developers can easily integrate audio handling into their applications using the Catalyst framework.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include <catalyst/audio/engine.hpp>

namespace catalyst::audio {

const char* module_name();

} // namespace catalyst::audio
