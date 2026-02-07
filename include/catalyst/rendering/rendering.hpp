/*
 * @file rendering.hpp
 * @brief Public API for the Catalyst Rendering library. This header defines the public interface for the Catalyst Rendering library, which provides functionality for rendering graphics using various backends. The functions declared in this header allow users to interact with the rendering system, query information about the active rendering backend, and perform other rendering-related operations. By including this header, developers can access the features of the Catalyst Rendering library in a platform-agnostic way, without needing to worry about the specific details of the underlying rendering implementations.
 * @details The Catalyst Rendering library is designed to provide a flexible and efficient rendering solution that can support multiple backends (e.g., OpenGL, Vulkan, DirectX) while maintaining a consistent API for users. The functions declared in this header serve as the primary entry points for interacting with the rendering system, allowing developers to query information about the active backend, create and manage rendering resources, and perform rendering operations. The implementation of these functions will typically involve calling into backend-specific code that handles the actual rendering logic, while the public API defined in this header abstracts away those details to provide a clean and user-friendly interface.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

/**
 * @namespace catalyst::rendering
 * @brief The catalyst::rendering namespace contains all the types and functions related to rendering graphics in the Catalyst framework. This includes various rendering utilities, resource management tools, and backend-specific implementations for rendering operations. By organizing all rendering-related functionality within this namespace, we can avoid naming conflicts and provide a clear structure for users of the library to access the various rendering tools they need for their applications. The Catalyst Rendering library is designed to be efficient, easy to use, and compatible with modern C++ standards, making it a valuable resource for developers building real-time applications and games that require high-quality graphics rendering.
 * @details The Catalyst Rendering library provides a collection of types and functions that are commonly used in graphics rendering. By including this header, users can access all the functionality provided by the Catalyst Rendering library without needing to include individual component headers. This allows developers to focus on building their applications while still having access to powerful tools for managing rendering resources, performing rendering operations, and interacting with various rendering backends effectively.
 */
namespace catalyst::rendering
{

    /**
     * @fn module_name
     * @brief Returns the name of this module as a string. This can be used for logging, debugging, or any situation where you want to identify the module by name.
     * @return A string literal representing the name of this module.
     */
    const char *module_name();

} // namespace catalyst::rendering
