/**
 * @file view.hpp
 * @brief View matrix utilities for Catalyst Math library.
 * @details This header provides functions for constructing look-at view matrices commonly used in 3D graphics applications. The functions allow for creating view matrices based on camera position, target point, and up vector, with support for both right-handed and left-handed coordinate systems. The resulting view matrices are in column-major order and can be used in graphics APIs that expect this format for transformation matrices. By using these utilities, developers can easily generate view matrices that are compatible with their rendering pipeline and coordinate system conventions.
 * License: MIT (see LICENSE).
 */

#pragma once

#include <catalyst/math/mat.hpp>
#include <catalyst/math/vec.hpp>

#include <concepts>
#include <type_traits>

/**
 * @namespace catalyst::math
 * @brief The catalyst::math namespace contains all the mathematical types and functions provided by the Catalyst Math library. This includes vector and matrix types, quaternion support, angle types with user-defined literals, projection matrix utilities, transformation functions, and more. By organizing all math-related functionality within this namespace, we can avoid naming conflicts and provide a clear structure for users of the library to access the various mathematical tools they need for graphics programming and game development.
 * @details The Catalyst Math library is designed to be efficient, easy to use, and compatible with modern C++ standards. It provides a comprehensive set of mathematical utilities that are commonly used in graphics applications, making it easier for developers to perform complex mathematical operations without having to implement them from scratch. By including the appropriate headers from the Catalyst Math library, users can access all the functionality they need for their projects.
 */
namespace catalyst::math
{

    /**
     * @fn look_at_rh
     * @brief Constructs a right-handed look-at view matrix. This function generates a view matrix for a camera in a right-handed coordinate system, where the forward direction is typically along the -Z axis in view space. The function takes the camera's position (eye), the target point it is looking at (target), and the up vector that defines the camera's orientation. The resulting view matrix can be used to transform world coordinates into view space for rendering.
     * @param eye A vec<T, 3> representing the position of the camera in world space.
     * @param target A vec<T, 3> representing the point in world space that the camera is looking at.
     * @param up A vec<T, 3> representing the up direction for the camera, which helps define the camera's orientation.
     * @tparam T The scalar type for the components of the input vectors and the resulting view matrix (e.g. float, double).
     * @return A 4x4 matrix that represents the right-handed look-at view transformation. The matrix is constructed such that the camera's forward direction points towards the target, the right direction is perpendicular to both the forward and up vectors, and the up direction is perpendicular to both the forward and right vectors. The resulting matrix can be used in graphics applications to transform world coordinates into view space for rendering.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> look_at_rh(const vec<T, 3> &eye, const vec<T, 3> &target, const vec<T, 3> &up) noexcept
    {
        const vec<T, 3> f = (target - eye).normalized();
        const vec<T, 3> s = f.cross(up).normalized();
        const vec<T, 3> u = s.cross(f);

        mat<T, 4, 4> m = mat<T, 4, 4>::identity();

        // Columns are basis vectors.
        m(0, 0) = s.x;
        m(1, 0) = s.y;
        m(2, 0) = s.z;

        m(0, 1) = u.x;
        m(1, 1) = u.y;
        m(2, 1) = u.z;

        m(0, 2) = -f.x;
        m(1, 2) = -f.y;
        m(2, 2) = -f.z;

        m(0, 3) = -s.dot(eye);
        m(1, 3) = -u.dot(eye);
        m(2, 3) = f.dot(eye);

        return m;
    }

    /**
     * @fn look_at_lh
     * @brief Constructs a left-handed look-at view matrix. This function generates a view matrix for a camera in a left-handed coordinate system, where the forward direction is typically along the +Z axis in view space. The function takes the camera's position (eye), the target point it is looking at (target), and the up vector that defines the camera's orientation. The resulting view matrix can be used to transform world coordinates into view space for rendering.
     * @param eye A vec<T, 3> representing the position of the camera in world space.
     * @param target A vec<T, 3> representing the point in world space that the camera is looking at.
     * @param up A vec<T, 3> representing the up direction for the camera, which helps define the camera's orientation.
     * @tparam T The scalar type for the components of the input vectors and the resulting view matrix (e.g. float, double).
     * @return A 4x4 matrix that represents the left-handed look-at view transformation. The matrix is constructed such that the camera's forward direction points towards the target, the right direction is perpendicular to both the forward and up vectors, and the up direction is perpendicular to both the forward and right vectors. The resulting matrix can be used in graphics applications to transform world coordinates into view space for rendering.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> look_at_lh(const vec<T, 3> &eye, const vec<T, 3> &target, const vec<T, 3> &up) noexcept
    {
        const vec<T, 3> z = (target - eye).normalized();
        const vec<T, 3> x = up.cross(z).normalized();
        const vec<T, 3> y = z.cross(x);

        mat<T, 4, 4> m = mat<T, 4, 4>::identity();

        m(0, 0) = x.x;
        m(1, 0) = x.y;
        m(2, 0) = x.z;

        m(0, 1) = y.x;
        m(1, 1) = y.y;
        m(2, 1) = y.z;

        m(0, 2) = z.x;
        m(1, 2) = z.y;
        m(2, 2) = z.z;

        m(0, 3) = -x.dot(eye);
        m(1, 3) = -y.dot(eye);
        m(2, 3) = -z.dot(eye);

        return m;
    }

} // namespace catalyst::math
