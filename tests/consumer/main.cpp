/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief What a consumer of an installed Catalyst can rely on. See the CMakeLists.txt beside this.
 * @details Everything here comes through the install prefix: the generated <catalyst/config.hpp>,
 * the umbrella header (which must compile against the installed headers alone, so an install that
 * left a module's headers out fails here), and one call into a linked module.
 */

#include <catalyst/catalyst.hpp>
#include <catalyst/config.hpp>
#include <catalyst/math/geometry.hpp>
#include <catalyst/math/vector.hpp>

#include <cstdio>
#include <string_view>

static_assert(CATALYST_VERSION ==
              CATALYST_VERSION_ENCODE(CATALYST_VERSION_MAJOR, CATALYST_VERSION_MINOR, CATALYST_VERSION_PATCH));
static_assert(CATALYST_VERSION >= CATALYST_VERSION_ENCODE(0, 1, 0));
static_assert(CATALYST_HAS_MATH, "the fixture requires catalyst::math");
static_assert(CATALYST_HAS_EVENTS, "the fixture requires catalyst::events");
static_assert(CATALYST_HAS_CORE, "events depends on core, so an install with events has core");

namespace
{
    int check(bool ok, const char *what)
    {
        std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
        return ok ? 0 : 1;
    }
} // namespace

int main()
{
    int failures = 0;

    failures += check(catalyst::math::magnitude(catalyst::math::vec3f{0.0f, 1.0f, 0.0f}) == 1.0f,
                      "catalyst::math is usable from the install");

    failures += check(std::string_view{CATALYST_RENDERING_BACKEND_NAME} != "auto",
                      "config.hpp records a resolved rendering backend");

#if CATALYST_CONSUMER_HAS_MONOLITH
    failures += check(std::string_view{catalyst::version()} == CATALYST_VERSION_STRING,
                      "catalyst::version() matches CATALYST_VERSION_STRING");
#endif

    return failures == 0 ? 0 : 1;
}
