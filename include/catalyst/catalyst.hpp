/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Every module this build of Catalyst contains.
 * @details Each include is guarded by the matching `CATALYST_HAS_*` from the generated
 * <catalyst/config.hpp>, so this header describes the library that was actually built. It used to
 * include all fourteen module headers unconditionally, whatever the `CATALYST_BUILD_*` options
 * said -- harmless only for as long as every switched-off module was a header-only stub, and a link
 * error the moment one of them grew a real symbol.
 *
 * Prefer a module's own header when you only need one; this one exists for convenience, not speed.
 */

#pragma once

#include <catalyst/config.hpp>

#if CATALYST_HAS_CORE
#include <catalyst/core/core.hpp>
#endif

#if CATALYST_HAS_EVENTS
#include <catalyst/events/bus.hpp>
#endif

#if CATALYST_HAS_TEXT
#include <catalyst/text/text.hpp>
#endif

#if CATALYST_HAS_ANIMATION
#include <catalyst/animation/animation.hpp>
#endif

#if CATALYST_HAS_AUDIO
#include <catalyst/audio/audio.hpp>
#endif

#if CATALYST_HAS_INPUT
#include <catalyst/input/input.hpp>
#endif

#if CATALYST_HAS_LOGGING
#include <catalyst/logging/logging.hpp>
#endif

#if CATALYST_HAS_MATH
#include <catalyst/math/math.hpp>
#endif

#if CATALYST_HAS_NET
#include <catalyst/net/net.hpp>
#endif

#if CATALYST_HAS_PHYSICS
#include <catalyst/physics/physics.hpp>
#endif

#if CATALYST_HAS_PLATFORM
#include <catalyst/platform/platform.hpp>
#endif

#if CATALYST_HAS_RENDERING
#include <catalyst/rendering/rendering.hpp>
#endif

#if CATALYST_HAS_RESOURCE
#include <catalyst/resource/resource.hpp>
#endif

#if CATALYST_HAS_SCENE
#include <catalyst/scene/scene.hpp>
#endif

#if CATALYST_HAS_UI
#include <catalyst/ui/ui.hpp>
#endif

#if CATALYST_HAS_UTILS
#include <catalyst/utils/utils.hpp>
#endif

namespace catalyst
{

    /** @brief The version of Catalyst this was built against, as a string. Matches
     * @ref CATALYST_VERSION_STRING. */
    [[nodiscard]] const char *version() noexcept;

} // namespace catalyst
