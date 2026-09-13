/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Opt-in short spelling: `math::vec3f` for `catalyst::math::vec3f`.
 * @details The module's types live in `catalyst::math`, like every other Catalyst module. They used
 * to live in a global `::math`, which meant that including any math header put `vec2f`, `dot`,
 * `transform`, `normalized` and `geometry` into every consumer's global namespace whether they
 * wanted them there or not -- names generic enough to collide with another library's, or the
 * consumer's own.
 *
 * The short spelling is still convenient in code that is dense with vector maths, so it stays
 * available -- as something a translation unit asks for, rather than something every include
 * imposes. Include this header where you want it, and nowhere else.
 *
 * @code
 * #include <catalyst/math/vector.hpp>
 * #include <catalyst/math/alias.hpp>
 *
 * math::vec3f up{0.0f, 1.0f, 0.0f};  // same type as catalyst::math::vec3f
 * @endcode
 *
 * @warning Never include this from a public header. A namespace alias at global scope is visible to
 * everything that includes it, transitively, and imposing it on a consumer is the exact problem
 * this arrangement exists to undo.
 */

#pragma once

namespace catalyst::math
{
}

/** @brief Short alias for @ref catalyst::math. */
namespace math = catalyst::math;
