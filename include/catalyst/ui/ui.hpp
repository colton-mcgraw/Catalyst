/**
 * @file ui.hpp
 * @brief Main header for the catalyst::ui module, which includes types and functions related to user interface measurements and layout.
 * @details This header includes the measurement.hpp file, which defines types and functions for representing and resolving CSS-like measurements (e.g. lengths with various units). The catalyst::ui namespace provides a structured way to access these UI-related measurement functionalities.
 * License: MIT (see LICENSE).
 */

#pragma once

#include <catalyst/ui/measurement.hpp>

/**
 * @namespace catalyst::ui
 * @brief The catalyst::ui namespace contains all the types and functions related to user interface measurements and layout. This includes various measurement types (e.g. length, percentage) and functions for resolving these measurements in the context of UI layout. By organizing all UI-related functionality within this namespace, we can avoid naming conflicts and provide a clear structure for users of the library to access the various tools they need for building user interfaces in their applications.
 * @details The catalyst::ui module provides a collection of types and functions that are commonly used in user interface development, particularly for handling measurements and layout. By including this header, users can access all the functionality provided by the catalyst::ui module without needing to include individual component headers. This allows developers to focus on building their user interfaces while still having access to powerful tools for managing measurements and layout effectively.
 */
namespace catalyst::ui
{

    /**
     * @fn module_name
     * @brief Returns the name of this module as a string. This can be used for logging, debugging, or any situation where you want to identify the module by name.
     * @return A string literal representing the name of this module.
     */
    const char *module_name();

} // namespace catalyst::ui
