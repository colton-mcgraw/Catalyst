/**
 * @file math.hpp
 * @brief Main header for Catalyst Math library, including all math components.
 * @details This header serves as the primary include for the Catalyst Math library, which provides a collection of mathematical types and functions commonly used in graphics programming and game development. It includes vector and matrix types, quaternion support, angle types with user-defined literals, projection matrix utilities, and more. By including this single header, users can access all the functionality provided by the Catalyst Math library without needing to include individual component headers. The library is designed to be efficient, easy to use, and compatible with modern C++ standards.
 * License: MIT (see LICENSE).
 */

#pragma once

#include <catalyst/math/simd.hpp>
#include <catalyst/math/vec.hpp>
#include <catalyst/math/mat.hpp>
#include <catalyst/math/quat.hpp>
#include <catalyst/math/euler.hpp>
#include <catalyst/math/projection.hpp>
#include <catalyst/math/transform.hpp>
#include <catalyst/math/view.hpp>
#include <catalyst/math/rect.hpp>

/**
 * @namespace catalyst::math
 * @brief The catalyst::math namespace contains all the mathematical types and functions provided by the Catalyst Math library. This includes vector and matrix types, quaternion support, angle types with user-defined literals, projection matrix utilities, transformation functions, and more. By organizing all math-related functionality within this namespace, we can avoid naming conflicts and provide a clear structure for users of the library to access the various mathematical tools they need for graphics programming and game development.
 * @details The Catalyst Math library is designed to be efficient, easy to use, and compatible with modern C++ standards. It provides a comprehensive set of mathematical utilities that are commonly used in graphics applications, making it easier for developers to perform complex mathematical operations without having to implement them from scratch. By including the appropriate headers from the Catalyst Math library, users can access all the functionality they need for their projects.
 */
namespace catalyst::math
{

    /**
     * @fn module_name
     * @brief Returns the name of the Catalyst Math module as a string literal.
     */
    const char *module_name();

} // namespace catalyst::math
