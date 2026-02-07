/**
 * @file core.hpp
 * @brief Main header for the catalyst::core module, which includes core types and functions related to event handling and dispatching. This header includes the dispatcher.hpp, event_sink.hpp, event_queue.hpp, event.hpp, and subscription.hpp files, which define the main components of the event system in the Catalyst framework. The catalyst::core namespace provides a structured way to access these core functionalities.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include <catalyst/core/dispatcher.hpp>
#include <catalyst/core/event_sink.hpp>
#include <catalyst/core/event_queue.hpp>
#include <catalyst/core/event.hpp>
#include <catalyst/core/subscription.hpp>

namespace catalyst::core
{

    /**
     * @fn module_name
     * @brief Returns the name of this module as a string. This can be used for logging, debugging, or any situation where you want to identify the module by name.
     * @return A string literal representing the name of this module.
     */
    const char *module_name();

} // namespace catalyst::core
