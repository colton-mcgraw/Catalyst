/**
 * @file core.hpp
 * @brief Main header for the catalyst::core module.
 * License: MIT (see LICENSE).
 */

#pragma once

namespace catalyst::core
{

    /**
     * @fn module_name
     * @brief Returns the name of this module as a string. This can be used for logging, debugging, or any situation
     * where you want to identify the module by name.
     * @return A string literal representing the name of this module.
     */
    const char *module_name();

} // namespace catalyst::core

namespace catalyst
{

    /**
     * @fn version
     * @brief The version of Catalyst this was built from, as a string -- the same value as
     * @ref CATALYST_VERSION_STRING in the generated <catalyst/config.hpp>.
     * @details Here rather than in the monolithic library's translation unit, where it used to
     * live: a build with CATALYST_BUILD_ALL=OFF has a version too, and asking for it should not
     * require linking every module.
     * @return A string literal, e.g. `"0.1.0"`.
     */
    [[nodiscard]] const char *version() noexcept;

} // namespace catalyst
