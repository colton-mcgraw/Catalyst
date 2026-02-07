/*
 * @file platform.cpp
 * @brief Implementation of the Catalyst Platform module, which provides an abstraction layer for platform-specific functionality. This file contains the implementation of functions that allow users to query information about the underlying platform and its capabilities, such as the name of the backend being used. By providing a consistent interface for accessing platform information, this module helps to ensure that applications built with Catalyst can run smoothly across different platforms without needing to worry about platform-specific details.
 * @details The Catalyst Platform module serves as an abstraction layer that allows developers to write cross-platform applications without needing to worry about the specific details of each platform. By providing functions that allow users to query information about the underlying platform, such as the name of the backend being used, this module helps to ensure that applications built with Catalyst can run smoothly across different platforms. The implementation of these functions is typically done in separate source files that are specific to each platform (e.g., Windows, Linux, macOS), allowing for clean separation of platform-specific code while still providing a unified interface for users of the library.
 */

#include <catalyst/platform/platform.hpp>

#include "detail_backend.hpp"

/**
 * @namespace catalyst::platform
 * @brief The catalyst::platform namespace contains all the platform-specific types and functions provided by the Catalyst Platform library. This includes window management, event handling, and other platform-related utilities. By organizing all platform-specific functionality within this namespace, we can avoid naming conflicts and provide a clear structure for users of the library to access the various platform tools they need for their applications. The Catalyst Platform library is designed to be efficient, easy to use, and compatible with modern C++ standards, making it a valuable resource for developers building real-time applications and games.
 * @details The Catalyst Platform library provides a collection of platform utilities and types commonly used in game development and real-time applications. By including the appropriate headers from the Catalyst Platform library, users can access all the functionality they need for their projects, such as creating windows, handling events, and interacting with the underlying operating system in a platform-agnostic way. This allows developers to focus on building their applications without worrying about the complexities of platform-specific code, while still having access to powerful tools for managing windows and events effectively.
 */
namespace catalyst::platform
{

  /**
   * @fn module_name
   * @brief Returns the name of this module as a string. This can be used for logging, debugging, or any situation where you want to identify the module by name.
   * @return A string literal representing the name of this module.
   */
  const char *module_name()
  {
    return detail::backend_name();
  }

} // namespace catalyst::platform
