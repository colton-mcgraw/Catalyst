/**
 * @file projection.hpp
 * @brief Projection matrix utilities for Catalyst Math library.
 * @details This header provides functions for constructing perspective projection matrices commonly used in 3D graphics applications. The functions allow for creating perspective projection matrices based on field of view, aspect ratio, near and far clipping planes, and options for handedness and depth range. The library supports both right-handed and left-handed coordinate systems, as well as depth ranges of 0 to 1 (D3D style) and -1 to 1 (OpenGL style). By using these utilities, developers can easily generate projection matrices that are compatible with their rendering pipeline and coordinate system conventions.
 * License: MIT (see LICENSE).
 */
#pragma once

#include <catalyst/math/euler.hpp>
#include <catalyst/math/mat.hpp>

#include <concepts>
#include <cmath>
#include <cstddef>
#include <type_traits>

/**
 * @namespace catalyst::math
 * @brief The catalyst::math namespace contains all the mathematical types and functions provided by the Catalyst Math library. This includes vector and matrix types, quaternion support, angle types with user-defined literals, projection matrix utilities, transformation functions, and more. By organizing all math-related functionality within this namespace, we can avoid naming conflicts and provide a clear structure for users of the library to access the various mathematical tools they need for graphics programming and game development.
 * @details The Catalyst Math library is designed to be efficient, easy to use, and compatible with modern C++ standards. It provides a comprehensive set of mathematical utilities that are commonly used in graphics applications, making it easier for developers to perform complex mathematical operations without having to implement them from scratch. By including the appropriate headers from the Catalyst Math library, users can access all the functionality they need for their projects.
 */
namespace catalyst::math
{

    /**
     * @enum handedness
     * @brief Enumeration to specify the handedness of the coordinate system for projection matrices.
     */
    enum class handedness
    {
        /**
         * @enum handedness::right
         * @brief Indicates a right-handed coordinate system, where the forward direction is typically along the -Z axis in view space. This is the most common convention used in graphics applications and APIs.
         */
        right,
        /**
         * @enum handedness::left
         * @brief Indicates a left-handed coordinate system, where the forward direction is typically along the +Z axis in view space.
         */
        left,
    };

    /**
     * @enum depth_range
     * @brief Enumeration to specify the depth range convention for projection matrices.
     */
    enum class depth_range
    {
        /**
         * @enum depth_range::zero_to_one
         * @brief Indicates a depth range convention where the normalized device coordinate (NDC) Z values are mapped to the range [0, 1]. This is commonly used in Direct3D and Vulkan APIs.
         */
        zero_to_one,
        /**
         * @enum depth_range::neg_one_to_one
         * @brief Indicates a depth range convention where the normalized device coordinate (NDC) Z values are mapped to the range [-1, 1]. This is commonly used in OpenGL and WebGL APIs.
         */
        neg_one_to_one,
    };

    namespace detail
    {
        /**
         * @fn tan_half_angle
         * @brief Computes the tangent of half the given angle in radians. This is a common calculation used in the construction of perspective projection matrices, where the field of view is often specified as an angle and the tangent of half that angle is used to determine the scaling factors for the X and Y axes. The function takes an angle in radians and returns the tangent of half that angle, which can be used in the perspective projection matrix calculations to achieve the desired field of view.
         * @param angle_radians The input angle in radians for which to compute the tangent of half the angle.
         * @tparam T Scalar type for the angle (e.g. float, double).
         * @return The tangent of half the input angle, which is a value used in perspective projection matrix calculations to determine the scaling factors for the X and Y axes based on the field of view.
         */
        template <typename T>
        [[nodiscard]] inline T tan_half_angle(T angle_radians) noexcept
        {
            const long double a = static_cast<long double>(angle_radians);
            return static_cast<T>(std::tan(a * 0.5L));
        }
    } // namespace detail

    /**
     * @fn perspective_fov_y
     * @brief Constructs a perspective projection matrix based on the given field of view in the Y direction, aspect ratio, near and far clipping planes, and options for handedness and depth range. This function allows for creating a perspective projection matrix that can be used in 3D graphics applications to transform 3D coordinates into clip space for rendering. The field of view is specified in radians, and the function provides flexibility in choosing between right-handed and left-handed coordinate systems, as well as different depth range conventions (0 to 1 or -1 to 1) to accommodate various graphics APIs and rendering pipelines.
     * @param fov_y_radians The field of view in the Y direction, specified in radians. This angle determines how wide the camera's view is and affects the perspective distortion of the rendered scene.
     * @param aspect The aspect ratio of the viewport, defined as the width divided by the height. This is used to ensure that the projection matrix correctly maps the 3D scene to the 2D viewport without distortion.
     * @param z_near The distance to the near clipping plane. Objects closer than this distance will not be rendered. This value must be greater than zero.
     * @param z_far The distance to the far clipping plane. Objects farther than this distance will not be rendered. This value must be greater than the near clipping plane distance.
     * @param hand The handedness of the coordinate system to use for the projection matrix. This can be either right-handed or left-handed, depending on the conventions of the graphics API being used.
     * @param depth The depth range convention to use for the projection matrix. This can be either zero_to_one (for Direct3D/Vulkan style) or neg_one_to_one (for OpenGL style), which determines how the Z values are mapped in normalized device coordinates.
     * @tparam T Scalar type for the matrix elements and parameters (e.g. float, double).
     * @return A 4x4 perspective projection matrix that can be used to transform 3D coordinates into clip space for rendering. The resulting matrix is constructed based on the specified field of view, aspect ratio, near and far clipping planes, and the chosen handedness and depth range conventions. This matrix can be used in graphics applications to achieve the desired perspective projection effect when rendering 3D scenes.
     * @note The function assumes that the input parameters are valid (e.g., positive field of view, aspect ratio, and clipping plane distances) and does not perform error checking on the inputs. It is the responsibility of the caller to ensure that the parameters are within a reasonable range to avoid issues such as division by zero or invalid projection matrices.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_fov_y(T fov_y_radians, T aspect, T z_near, T z_far,
                                                        handedness hand = handedness::right,
                                                        depth_range depth = depth_range::zero_to_one) noexcept
    {
        mat<T, 4, 4> m{};

        const T tan_half = detail::tan_half_angle(fov_y_radians);
        const T y_scale = T{1} / tan_half;
        const T x_scale = y_scale / aspect;

        m(0, 0) = x_scale;
        m(1, 1) = y_scale;

        const T zn = z_near;
        const T zf = z_far;

        if (hand == handedness::left)
        {
            if (depth == depth_range::zero_to_one)
            {
                m(2, 2) = zf / (zf - zn);
                m(3, 2) = T{1};
                m(2, 3) = -(zf * zn) / (zf - zn);
            }
            else
            {
                m(2, 2) = (zf + zn) / (zf - zn);
                m(3, 2) = T{1};
                m(2, 3) = -(T{2} * zf * zn) / (zf - zn);
            }
        }
        else
        {
            if (depth == depth_range::zero_to_one)
            {
                m(2, 2) = zf / (zn - zf);
                m(3, 2) = T{-1};
                m(2, 3) = (zf * zn) / (zn - zf);
            }
            else
            {
                m(2, 2) = (zf + zn) / (zn - zf);
                m(3, 2) = T{-1};
                m(2, 3) = (T{2} * zf * zn) / (zn - zf);
            }
        }

        return m;
    }

    /**
     * @fn perspective_fov_y_degrees
     * @brief Constructs a perspective projection matrix based on the given field of view in the Y direction specified in degrees, aspect ratio, near and far clipping planes, and options for handedness and depth range. This function is an overload of the perspective_fov_y function that accepts the field of view angle in degrees instead of radians. The function converts the input angle from degrees to radians before calling the perspective_fov_y function to create the projection matrix. The rotation is applied in the order of yaw (rotation around the Y-axis), then pitch (rotation around the X-axis), and finally roll (rotation around the Z-axis).
     * @param fov_y_degrees The field of view in the Y direction, specified in degrees. This angle determines how wide the camera's view is and affects the perspective distortion of the rendered scene.
     * @param aspect The aspect ratio of the viewport, defined as the width divided by the height. This is used to ensure that the projection matrix correctly maps the 3D scene to the 2D viewport without distortion.
     * @param z_near The distance to the near clipping plane. Objects closer than this distance will not be rendered. This value must be greater than zero.
     * @param z_far The distance to the far clipping plane. Objects farther than this distance will not be rendered. This value must be greater than the near clipping plane distance.
     * @param hand The handedness of the coordinate system to use for the projection matrix. This can be either right-handed or left-handed, depending on the conventions of the graphics API being used.
     * @param depth The depth range convention to use for the projection matrix. This can be either zero_to_one (for Direct3D/Vulkan style) or neg_one_to_one (for OpenGL style), which determines how the Z values are mapped in normalized device coordinates.
     * @tparam T Scalar type for the matrix elements and parameters (e.g. float, double).
     * @return A 4x4 perspective projection matrix that can be used to transform 3D coordinates into clip space for rendering. The resulting matrix is constructed based on the specified field of view, aspect ratio, near and far clipping planes, and the chosen handedness and depth range conventions. This function provides a convenient way to create a perspective projection matrix when working with angles in degrees, which are often more intuitive for human understanding and input.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_fov_y(radians<T> fov_y, T aspect, T z_near, T z_far,
                                                        handedness hand = handedness::right,
                                                        depth_range depth = depth_range::zero_to_one) noexcept
    {
        return perspective_fov_y(fov_y.count(), aspect, z_near, z_far, hand, depth);
    }

    /**
     * @fn perspective_fov_y_degrees
     * @brief Constructs a perspective projection matrix based on the given field of view in the Y direction specified as degrees<T>. This is an overload of the perspective_fov_y_degrees function that accepts the field of view angle wrapped in a degrees<T> type, which provides type safety and clarity when working with angles. The function converts the input angle from degrees to radians before calling the perspective_fov_y function to create the projection matrix. The rotation is applied in the order of yaw (rotation around the Y-axis), then pitch (rotation around the X-axis), and finally roll (rotation around the Z-axis).
     * @param fov_y_degrees The field of view in the Y direction, wrapped in a degrees<T> type. This angle determines how wide the camera's view is and affects the perspective distortion of the rendered scene.
     * @param aspect The aspect ratio of the viewport, defined as the width divided by the height. This is used to ensure that the projection matrix correctly maps the 3D scene to the 2D viewport without distortion.
     * @param z_near The distance to the near clipping plane. Objects closer than this distance will not be rendered. This value must be greater than zero.
     * @param z_far The distance to the far clipping plane. Objects farther than this distance will not be rendered. This value must be greater than the near clipping plane distance.
     * @param hand The handedness of the coordinate system to use for the projection matrix. This can be either right-handed or left-handed, depending on the conventions of the graphics API being used.
     * @param depth The depth range convention to use for the projection matrix. This can be either zero_to_one (for Direct3D/Vulkan style) or neg_one_to_one (for OpenGL style), which determines how the Z values are mapped in normalized device coordinates.
     * @tparam T Scalar type for the matrix elements and parameters (e.g. float, double).
     * @return A 4x4 perspective projection matrix that can be used to transform 3D coordinates into clip space for rendering. The resulting matrix is constructed based on the specified field of view, aspect ratio, near and far clipping planes, and the chosen handedness and depth range conventions. This overload allows for more explicit handling of angles by using the degrees<T> type, which can help prevent confusion and errors when working with angle units.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_fov_y_degrees(T fov_y_degrees, T aspect, T z_near, T z_far,
                                                                handedness hand = handedness::right,
                                                                depth_range depth = depth_range::zero_to_one) noexcept
    {
        return perspective_fov_y(deg_to_rad(fov_y_degrees), aspect, z_near, z_far, hand, depth);
    }

    /**
     * @fn perspective_fov_y_degrees
     * @brief Constructs a perspective projection matrix based on the given field of view in the Y direction specified as degrees<T>. This is an overload of the perspective_fov_y_degrees function that accepts the field of view angle wrapped in a degrees<T> type, which provides type safety and clarity when working with angles. The function converts the input angle from degrees to radians before calling the perspective_fov_y function to create the projection matrix. The rotation is applied in the order of yaw (rotation around the Y-axis), then pitch (rotation around the X-axis), and finally roll (rotation around the Z-axis).
     * @param fov_y_degrees The field of view in the Y direction, wrapped in a degrees<T> type. This angle determines how wide the camera's view is and affects the perspective distortion of the rendered scene.
     * @param aspect The aspect ratio of the viewport, defined as the width divided by the height. This is used to ensure that the projection matrix correctly maps the 3D scene to the 2D viewport without distortion.
     * @param z_near The distance to the near clipping plane. Objects closer than this distance will not be rendered. This value must be greater than zero.
     * @param z_far The distance to the far clipping plane. Objects farther than this distance will not be rendered. This value must be greater than the near clipping plane distance.
     * @param hand The handedness of the coordinate system to use for the projection matrix. This can be either right-handed or left-handed, depending on the conventions of the graphics API being used.
     * @param depth The depth range convention to use for the projection matrix. This can be either zero_to_one (for Direct3D/Vulkan style) or neg_one_to_one (for OpenGL style), which determines how the Z values are mapped in normalized device coordinates.
     * @tparam T Scalar type for the matrix elements and parameters (e.g. float, double).
     * @return A 4x4 perspective projection matrix that can be used to transform 3D coordinates into clip space for rendering. The resulting matrix is constructed based on the specified field of view, aspect ratio, near and far clipping planes, and the chosen handedness and depth range conventions. This overload allows for more explicit handling of angles by using the degrees<T> type, which can help prevent confusion and errors when working with angle units.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_fov_y_degrees(degrees<T> fov_y_degrees, T aspect, T z_near, T z_far,
                                                                handedness hand = handedness::right,
                                                                depth_range depth = depth_range::zero_to_one) noexcept
    {
        return perspective_fov_y(to_radians(fov_y_degrees).count(), aspect, z_near, z_far, hand, depth);
    }

    /**
     * @fn perspective
     * @brief Constructs a perspective projection matrix with default parameters for handedness and depth range. This function is a convenience overload of the perspective_fov_y function that assumes a right-handed coordinate system and a depth range of 0 to 1 (D3D style). It allows for quickly creating a perspective projection matrix by only specifying the field of view in the Y direction, aspect ratio, and near and far clipping planes, while using common defaults for the other parameters. The field of view is specified in radians, and the resulting projection matrix can be used in graphics applications to achieve the desired perspective projection effect when rendering 3D scenes.
     * @param fov_y_radians The field of view in the Y direction, specified in radians. This angle determines how wide the camera's view is and affects the perspective distortion of the rendered scene.
     * @param aspect The aspect ratio of the viewport, defined as the width divided by the height. This is used to ensure that the projection matrix correctly maps the 3D scene to the 2D viewport without distortion.
     * @param z_near The distance to the near clipping plane. Objects closer than this distance will not be rendered. This value must be greater than zero.
     * @param z_far The distance to the far clipping plane. Objects farther than this distance will not be rendered. This value must be greater than the near clipping plane distance.
     * @tparam T Scalar type for the matrix elements and parameters (e.g. float, double).
     * @return A 4x4 perspective projection matrix that can be used to transform 3D coordinates into clip space for rendering. The resulting matrix is constructed based on the specified field of view, aspect ratio, near and far clipping planes, and uses default values for handedness (right-handed) and depth range (0 to 1). This function provides a convenient way to create a perspective projection matrix with common defaults, making it easier for developers to set up their projection matrices without needing to specify all parameters explicitly.
     * @note The function assumes that the input parameters are valid (e.g., positive field of view, aspect ratio, and clipping plane distances) and does not perform error checking on the inputs. It is the responsibility of the caller to ensure that the parameters are within a reasonable range to avoid issues such as division by zero or invalid projection matrices.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective(T fov_y_radians, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y(fov_y_radians, aspect, z_near, z_far, handedness::right, depth_range::zero_to_one);
    }

    /**
     * @fn perspective
     * @brief Constructs a perspective projection matrix with default parameters for handedness and depth range. This function is a convenience overload of the perspective_fov_y function that assumes a right-handed coordinate system and a depth range of 0 to 1 (D3D style). It allows for quickly creating a perspective projection matrix by only specifying the field of view in the Y direction, aspect ratio, and near and far clipping planes, while using common defaults for the other parameters. The field of view is specified in radians, and the resulting projection matrix can be used in graphics applications to achieve the desired perspective projection effect when rendering 3D scenes.
     * @param fov_y The field of view in the Y direction, wrapped in a radians<T> type. This angle determines how wide the camera's view is and affects the perspective distortion of the rendered scene.
     * @param aspect The aspect ratio of the viewport, defined as the width divided by the height. This is used to ensure that the projection matrix correctly maps the 3D scene to the 2D viewport without distortion.
     * @param z_near The distance to the near clipping plane. Objects closer than this distance will not be rendered. This value must be greater than zero.
     * @param z_far The distance to the far clipping plane. Objects farther than this distance will not be rendered. This value must be greater than the near clipping plane distance.
     * @tparam T Scalar type for the matrix elements and parameters (e.g. float, double).
     * @return A 4x4 perspective projection matrix that can be used to transform 3D coordinates into clip space for rendering. The resulting matrix is constructed based on the specified field of view, aspect ratio, near and far clipping planes, and uses default values for handedness (right-handed) and depth range (0 to 1). This function provides a convenient way to create a perspective projection matrix with common defaults, making it easier for developers to set up their projection matrices without needing to specify all parameters explicitly.
     * @note The function assumes that the input parameters are valid (e.g., positive field of view, aspect ratio, and clipping plane distances) and does not perform error checking on the inputs. It is the responsibility of the caller to ensure that the parameters are within a reasonable range to avoid issues such as division by zero or invalid projection matrices.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective(radians<T> fov_y, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y(fov_y.count(), aspect, z_near, z_far, handedness::right, depth_range::zero_to_one);
    }

    /**
     * @fn perspective_degrees
     * @brief Constructs a perspective projection matrix with default parameters for handedness and depth range, and accepts the field of view in the Y direction specified in degrees. This function is a convenience overload of the perspective_fov_y_degrees function that assumes a right-handed coordinate system and a depth range of 0 to 1 (D3D style). It allows for quickly creating a perspective projection matrix by only specifying the field of view in degrees, aspect ratio, and near and far clipping planes, while using common defaults for the other parameters. The resulting projection matrix can be used in graphics applications to achieve the desired perspective projection effect when rendering 3D scenes.
     * @param fov_y_degrees The field of view in the Y direction, specified in degrees. This angle determines how wide the camera's view is and affects the perspective distortion of the rendered scene.
     * @param aspect The aspect ratio of the viewport, defined as the width divided by the height. This is used to ensure that the projection matrix correctly maps the 3D scene to the 2D viewport without distortion.
     * @param z_near The distance to the near clipping plane. Objects closer than this distance will not be rendered. This value must be greater than zero.
     * @param z_far The distance to the far clipping plane. Objects farther than this distance will not be rendered. This value must be greater than the near clipping plane distance.
     * @tparam T Scalar type for the matrix elements and parameters (e.g. float, double).
     * @return A 4x4 perspective projection matrix that can be used to transform 3D coordinates into clip space for rendering. The resulting matrix is constructed based on the specified field of view, aspect ratio, near and far clipping planes, and uses default values for handedness (right-handed) and depth range (0 to 1). This function provides a convenient way to create a perspective projection matrix with common defaults, making it easier for developers to set up their projection matrices without needing to specify all parameters explicitly. The overload that accepts the field of view in degrees allows for more intuitive input when working with angles, as degrees are often more familiar to developers than radians.
     * @note The function assumes that the input parameters are valid (e.g., positive field of view, aspect ratio, and clipping plane distances) and does not perform error checking on the inputs. It is the responsibility of the caller to ensure that the parameters are within a reasonable range to avoid issues such as division by zero or invalid projection matrices.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_degrees(T fov_y_degrees, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y_degrees(fov_y_degrees, aspect, z_near, z_far, handedness::right, depth_range::zero_to_one);
    }

    /**
     * @fn perspective_degrees
     * @brief Constructs a perspective projection matrix with default parameters for handedness and depth range, and accepts the field of view in the Y direction specified in degrees. This function is a convenience overload of the perspective_fov_y_degrees function that assumes a right-handed coordinate system and a depth range of 0 to 1 (D3D style). It allows for quickly creating a perspective projection matrix by only specifying the field of view in degrees, aspect ratio, and near and far clipping planes, while using common defaults for the other parameters. The resulting projection matrix can be used in graphics applications to achieve the desired perspective projection effect when rendering 3D scenes.
     * @param fov_y_degrees The field of view in the Y direction, wrapped in a degrees<T> type. This angle determines how wide the camera's view is and affects the perspective distortion of the rendered scene.
     * @param aspect The aspect ratio of the viewport, defined as the width divided by the height. This is used to ensure that the projection matrix correctly maps the 3D scene to the 2D viewport without distortion.
     * @param z_near The distance to the near clipping plane. Objects closer than this distance will not be rendered. This value must be greater than zero.
     * @param z_far The distance to the far clipping plane. Objects farther than this distance will not be rendered. This value must be greater than the near clipping plane distance.
     * @tparam T Scalar type for the matrix elements and parameters (e.g. float, double).
     * @return A 4x4 perspective projection matrix that can be used to transform 3D coordinates into clip space for rendering. The resulting matrix is constructed based on the specified field of view, aspect ratio, near and far clipping planes, and uses default values for handedness (right-handed) and depth range (0 to 1). This function provides a convenient way to create a perspective projection matrix with common defaults, making it easier for developers to set up their projection matrices without needing to specify all parameters explicitly. The overload that accepts the field of view in degrees allows for more intuitive input when working with angles, as degrees are often more familiar to developers than radians.
     * @note The function assumes that the input parameters are valid (e.g., positive field of view, aspect ratio, and clipping plane distances) and does not perform error checking on the inputs. It is the responsibility of the caller to ensure that the parameters are within a reasonable range to avoid issues such as division by zero or invalid projection matrices.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_degrees(degrees<T> fov_y_degrees, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y_degrees(fov_y_degrees, aspect, z_near, z_far, handedness::right, depth_range::zero_to_one);
    }

    /**
     * @fn perspective_rh_zo
     * @brief Constructs a right-handed perspective projection matrix with a depth range of 0 to 1 (D3D style). This function is a convenience overload of the perspective_fov_y function that assumes a right-handed coordinate system and a depth range of 0 to 1. It allows for quickly creating a perspective projection matrix by only specifying the field of view in the Y direction, aspect ratio, and near and far clipping planes, while using common defaults for the other parameters. The field of view is specified in radians, and the resulting projection matrix can be used in graphics applications to achieve the desired perspective projection effect when rendering 3D scenes using a right-handed coordinate system and a depth range suitable for Direct3D or Vulkan APIs.
     * @param fov_y_radians The field of view in the Y direction, specified in radians. This angle determines how wide the camera's view is and affects the perspective distortion of the rendered scene.
     * @param aspect The aspect ratio of the viewport, defined as the width divided by the height. This is used to ensure that the projection matrix correctly maps the 3D scene to the 2D viewport without distortion.
     * @param z_near The distance to the near clipping plane. Objects closer than this distance will not be rendered. This value must be greater than zero.
     * @param z_far The distance to the far clipping plane. Objects farther than this distance will not be rendered. This value must be greater than the near clipping plane distance.
     * @tparam T Scalar type for the matrix elements and parameters (e.g. float, double).
     * @return A 4x4 perspective projection matrix that can be used to transform 3D coordinates into clip space for rendering. The resulting matrix is constructed based on the specified field of view, aspect ratio, near and far clipping planes, and uses a right-handed coordinate system with a depth range of 0 to 1. This function provides a convenient way to create a perspective projection matrix that is compatible with Direct3D or Vulkan APIs, which commonly use this convention for their projection matrices.
     * @note The function assumes that the input parameters are valid (e.g., positive field of view, aspect ratio, and clipping plane distances) and does not perform error checking on the inputs. It is the responsibility of the caller to ensure that the parameters are within a reasonable range to avoid issues such as division by zero or invalid projection matrices.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_rh_zo(T fov_y_radians, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y(fov_y_radians, aspect, z_near, z_far, handedness::right, depth_range::zero_to_one);
    }

    /**
     * @fn perspective_rh_zo
     * @brief Constructs a right-handed perspective projection matrix with a depth range of 0 to 1 (D3D style). This function is a convenience overload of the perspective_fov_y function that assumes a right-handed coordinate system and a depth range of 0 to 1. It allows for quickly creating a perspective projection matrix by only specifying the field of view in the Y direction, aspect ratio, and near and far clipping planes, while using common defaults for the other parameters. The field of view is specified in radians, and the resulting projection matrix can be used in graphics applications to achieve the desired perspective projection effect when rendering 3D scenes using a right-handed coordinate system and a depth range suitable for Direct3D or Vulkan APIs.
     * @param fov_y The field of view in the Y direction, wrapped in a radians<T> type. This angle determines how wide the camera's view is and affects the perspective distortion of the rendered scene.
     * @param aspect The aspect ratio of the viewport, defined as the width divided by the height. This is used to ensure that the projection matrix correctly maps the 3D scene to the 2D viewport without distortion.
     * @param z_near The distance to the near clipping plane. Objects closer than this distance will not be rendered. This value must be greater than zero.
     * @param z_far The distance to the far clipping plane. Objects farther than this distance will not be rendered. This value must be greater than the near clipping plane distance.
     * @tparam T Scalar type for the matrix elements and parameters (e.g. float, double).
     * @return A 4x4 perspective projection matrix that can be used to transform 3D coordinates into clip space for rendering. The resulting matrix is constructed based on the specified field of view, aspect ratio, near and far clipping planes, and uses a right-handed coordinate system with a depth range of 0 to 1. This function provides a convenient way to create a perspective projection matrix that is compatible with Direct3D or Vulkan APIs, which commonly use this convention for their projection matrices.
     * @note The function assumes that the input parameters are valid (e.g., positive field of view, aspect ratio, and clipping plane distances) and does not perform error checking on the inputs. It is the responsibility of the caller to ensure that the parameters are within a reasonable range to avoid issues such as division by zero or invalid projection matrices.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_rh_zo(radians<T> fov_y, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y(fov_y.count(), aspect, z_near, z_far, handedness::right, depth_range::zero_to_one);
    }

    /**
     * @fn perspective_lh_zo
     * @brief Constructs a left-handed perspective projection matrix with a depth range of 0 to 1 (D3D style). This function is a convenience overload of the perspective_fov_y function that assumes a left-handed coordinate system and a depth range of 0 to 1. It allows for quickly creating a perspective projection matrix by only specifying the field of view in the Y direction, aspect ratio, and near and far clipping planes, while using common defaults for the other parameters. The field of view is specified in radians, and the resulting projection matrix can be used in graphics applications to achieve the desired perspective projection effect when rendering 3D scenes using a left-handed coordinate system and a depth range suitable for Direct3D or Vulkan APIs.
     * @param fov_y_radians The field of view in the Y direction, specified in radians. This angle determines how wide the camera's view is and affects the perspective distortion of the rendered scene.
     * @param aspect The aspect ratio of the viewport, defined as the width divided by the height. This is used to ensure that the projection matrix correctly maps the 3D scene to the 2D viewport without distortion.
     * @param z_near The distance to the near clipping plane. Objects closer than this distance will not be rendered. This value must be greater than zero.
     * @param z_far The distance to the far clipping plane. Objects farther than this distance will not be rendered. This value must be greater than the near clipping plane distance.
     * @tparam T Scalar type for the matrix elements and parameters (e.g. float, double).
     * @return A 4x4 perspective projection matrix that can be used to transform 3D coordinates into clip space for rendering. The resulting matrix is constructed based on the specified field of view, aspect ratio, near and far clipping planes, and uses a left-handed coordinate system with a depth range of 0 to 1. This function provides a convenient way to create a perspective projection matrix that is compatible with Direct3D or Vulkan APIs when using a left-handed coordinate system, which is an alternative convention for representing 3D transformations in graphics applications.
     * @note The function assumes that the input parameters are valid (e.g., positive field of view, aspect ratio, and clipping plane distances) and does not perform error checking on the inputs. It is the responsibility of the caller to ensure that the parameters are within a reasonable range to avoid issues such as division by zero or invalid projection matrices.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_lh_zo(T fov_y_radians, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y(fov_y_radians, aspect, z_near, z_far, handedness::left, depth_range::zero_to_one);
    }

    /**
     * @fn perspective_lh_zo
     * @brief Constructs a left-handed perspective projection matrix with a depth range of 0 to 1 (D3D style). This function is a convenience overload of the perspective_fov_y function that assumes a left-handed coordinate system and a depth range of 0 to 1. It allows for quickly creating a perspective projection matrix by only specifying the field of view in the Y direction, aspect ratio, and near and far clipping planes, while using common defaults for the other parameters. The field of view is specified in radians, and the resulting projection matrix can be used in graphics applications to achieve the desired perspective projection effect when rendering 3D scenes using a left-handed coordinate system and a depth range suitable for Direct3D or Vulkan APIs.
     * @param fov_y The field of view in the Y direction, wrapped in a radians<T> type. This angle determines how wide the camera's view is and affects the perspective distortion of the rendered scene.
     * @param aspect The aspect ratio of the viewport, defined as the width divided by the height. This is used to ensure that the projection matrix correctly maps the 3D scene to the 2D viewport without distortion.
     * @param z_near The distance to the near clipping plane. Objects closer than this distance will not be rendered. This value must be greater than zero.
     * @param z_far The distance to the far clipping plane. Objects farther than this distance will not be rendered. This value must be greater than the near clipping plane distance.
     * @tparam T Scalar type for the matrix elements and parameters (e.g. float, double).
     * @return A 4x4 perspective projection matrix that can be used to transform 3D coordinates into clip space for rendering. The resulting matrix is constructed based on the specified field of view, aspect ratio, near and far clipping planes, and uses a left-handed coordinate system with a depth range of 0 to 1. This function provides a convenient way to create a perspective projection matrix that is compatible with Direct3D or Vulkan APIs when using a left-handed coordinate system, which is an alternative convention for representing 3D transformations in graphics applications.
     * @note The function assumes that the input parameters are valid (e.g., positive field of view, aspect ratio, and clipping plane distances) and does not perform error checking on the inputs. It is the responsibility of the caller to ensure that the parameters are within a reasonable range to avoid issues such as division by zero or invalid projection matrices.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_lh_zo(radians<T> fov_y, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y(fov_y.count(), aspect, z_near, z_far, handedness::left, depth_range::zero_to_one);
    }

    /**
     * @fn perspective_rh_no
     * @brief Constructs a right-handed perspective projection matrix with a depth range of -1 to 1 (OpenGL style). This function is a convenience overload of the perspective_fov_y function that assumes a right-handed coordinate system and a depth range of -1 to 1. It allows for quickly creating a perspective projection matrix by only specifying the field of view in the Y direction, aspect ratio, and near and far clipping planes, while using common defaults for the other parameters. The field of view is specified in radians, and the resulting projection matrix can be used in graphics applications to achieve the desired perspective projection effect when rendering 3D scenes using a right-handed coordinate system and a depth range suitable for OpenGL APIs.
     * @param fov_y_radians The field of view in the Y direction, specified in radians. This angle determines how wide the camera's view is and affects the perspective distortion of the rendered scene.
     * @param aspect The aspect ratio of the viewport, defined as the width divided by the height. This is used to ensure that the projection matrix correctly maps the 3D scene to the 2D viewport without distortion.
     * @param z_near The distance to the near clipping plane. Objects closer than this distance will not be rendered. This value must be greater than zero.
     * @param z_far The distance to the far clipping plane. Objects farther than this distance will not be rendered. This value must be greater than the near clipping plane distance.
     * @tparam T Scalar type for the matrix elements and parameters (e.g. float, double).
     * @return A 4x4 perspective projection matrix that can be used to transform 3D coordinates into clip space for rendering. The resulting matrix is constructed based on the specified field of view, aspect ratio, near and far clipping planes, and uses a right-handed coordinate system with a depth range of -1 to 1. This function provides a convenient way to create a perspective projection matrix that is compatible with OpenGL APIs, which commonly use this convention for their projection matrices.
     * @note The function assumes that the input parameters are valid (e.g., positive field of view, aspect ratio, and clipping plane distances) and does not perform error checking on the inputs. It is the responsibility of the caller to ensure that the parameters are within a reasonable range to avoid issues such as division by zero or invalid projection matrices.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_rh_no(T fov_y_radians, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y(fov_y_radians, aspect, z_near, z_far, handedness::right, depth_range::neg_one_to_one);
    }

    /**
     * @fn perspective_rh_no
     * @brief Constructs a right-handed perspective projection matrix with a depth range of -1 to 1 (OpenGL style). This function is a convenience overload of the perspective_fov_y function that assumes a right-handed coordinate system and a depth range of -1 to 1. It allows for quickly creating a perspective projection matrix by only specifying the field of view in the Y direction, aspect ratio, and near and far clipping planes, while using common defaults for the other parameters. The field of view is specified in radians, and the resulting projection matrix can be used in graphics applications to achieve the desired perspective projection effect when rendering 3D scenes using a right-handed coordinate system and a depth range suitable for OpenGL APIs.
     * @param fov_y The field of view in the Y direction, wrapped in a radians<T> type. This angle determines how wide the camera's view is and affects the perspective distortion of the rendered scene.
     * @param aspect The aspect ratio of the viewport, defined as the width divided by the height. This is used to ensure that the projection matrix correctly maps the 3D scene to the 2D viewport without distortion.
     * @param z_near The distance to the near clipping plane. Objects closer than this distance will not be rendered. This value must be greater than zero.
     * @param z_far The distance to the far clipping plane. Objects farther than this distance will not be rendered. This value must be greater than the near clipping plane distance.
     * @tparam T Scalar type for the matrix elements and parameters (e.g. float, double).
     * @return A 4x4 perspective projection matrix that can be used to transform 3D coordinates into clip space for rendering. The resulting matrix is constructed based on the specified field of view, aspect ratio, near and far clipping planes, and uses a right-handed coordinate system with a depth range of -1 to 1. This function provides a convenient way to create a perspective projection matrix that is compatible with OpenGL APIs, which commonly use this convention for their projection matrices.
     * @note The function assumes that the input parameters are valid (e.g., positive field of view, aspect ratio, and clipping plane distances) and does not perform error checking on the inputs. It is the responsibility of the caller to ensure that the parameters are within a reasonable range to avoid issues such as division by zero or invalid projection matrices.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_rh_no(radians<T> fov_y, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y(fov_y.count(), aspect, z_near, z_far, handedness::right, depth_range::neg_one_to_one);
    }

    /**
     * @fn perspective_lh_no
     * @brief Constructs a left-handed perspective projection matrix with a depth range of -1 to 1 (OpenGL style). This function is a convenience overload of the perspective_fov_y function that assumes a left-handed coordinate system and a depth range of -1 to 1. It allows for quickly creating a perspective projection matrix by only specifying the field of view in the Y direction, aspect ratio, and near and far clipping planes, while using common defaults for the other parameters. The field of view is specified in radians, and the resulting projection matrix can be used in graphics applications to achieve the desired perspective projection effect when rendering 3D scenes using a left-handed coordinate system and a depth range suitable for OpenGL APIs.
     * @param fov_y_radians The field of view in the Y direction, specified in radians. This angle determines how wide the camera's view is and affects the perspective distortion of the rendered scene.
     * @param aspect The aspect ratio of the viewport, defined as the width divided by the height. This is used to ensure that the projection matrix correctly maps the 3D scene to the 2D viewport without distortion.
     * @param z_near The distance to the near clipping plane. Objects closer than this distance will not be rendered. This value must be greater than zero.
     * @param z_far The distance to the far clipping plane. Objects farther than this distance will not be rendered. This value must be greater than the near clipping plane distance.
     * @tparam T Scalar type for the matrix elements and parameters (e.g. float, double).
     * @return A 4x4 perspective projection matrix that can be used to transform 3D coordinates into clip space for rendering. The resulting matrix is constructed based on the specified field of view, aspect ratio, near and far clipping planes, and uses a left-handed coordinate system with a depth range of -1 to 1. This function provides a convenient way to create a perspective projection matrix that is compatible with OpenGL APIs when using a left-handed coordinate system, which is an alternative convention for representing 3D transformations in graphics applications.
     * @note The function assumes that the input parameters are valid (e.g., positive field of view, aspect ratio, and clipping plane distances) and does not perform error checking on the inputs. It is the responsibility of the caller to ensure that the parameters are within a reasonable range to avoid issues such as division by zero or invalid projection matrices.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_lh_no(T fov_y_radians, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y(fov_y_radians, aspect, z_near, z_far, handedness::left, depth_range::neg_one_to_one);
    }

    /**
     * @fn perspective_lh_no
     * @brief Constructs a left-handed perspective projection matrix with a depth range of -1 to 1 (OpenGL style). This function is a convenience overload of the perspective_fov_y function that assumes a left-handed coordinate system and a depth range of -1 to 1. It allows for quickly creating a perspective projection matrix by only specifying the field of view in the Y direction, aspect ratio, and near and far clipping planes, while using common defaults for the other parameters. The field of view is specified in radians, and the resulting projection matrix can be used in graphics applications to achieve the desired perspective projection effect when rendering 3D scenes using a left-handed coordinate system and a depth range suitable for OpenGL APIs.
     * @param fov_y The field of view in the Y direction, wrapped in a radians<T> type. This angle determines how wide the camera's view is and affects the perspective distortion of the rendered scene.
     * @param aspect The aspect ratio of the viewport, defined as the width divided by the height. This is used to ensure that the projection matrix correctly maps the 3D scene to the 2D viewport without distortion.
     * @param z_near The distance to the near clipping plane. Objects closer than this distance will not be rendered. This value must be greater than zero.
     * @param z_far The distance to the far clipping plane. Objects farther than this distance will not be rendered. This value must be greater than the near clipping plane distance.
     * @tparam T Scalar type for the matrix elements and parameters (e.g. float, double).
     * @return A 4x4 perspective projection matrix that can be used to transform 3D coordinates into clip space for rendering. The resulting matrix is constructed based on the specified field of view, aspect ratio, near and far clipping planes, and uses a left-handed coordinate system with a depth range of -1 to 1. This function provides a convenient way to create a perspective projection matrix that is compatible with OpenGL APIs when using a left-handed coordinate system, which is an alternative convention for representing 3D transformations in graphics applications.
     * @note The function assumes that the input parameters are valid (e.g., positive field of view, aspect ratio, and clipping plane distances) and does not perform error checking on the inputs. It is the responsibility of the caller to ensure that the parameters are within a reasonable range to avoid issues such as division by zero or invalid projection matrices.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> perspective_lh_no(radians<T> fov_y, T aspect, T z_near, T z_far) noexcept
    {
        return perspective_fov_y(fov_y.count(), aspect, z_near, z_far, handedness::left, depth_range::neg_one_to_one);
    }

    /**
     * @fn orthographic
     * @brief Constructs an orthographic projection matrix with default parameters for handedness and depth range. This function is a convenience overload of the orthographic function that assumes a right-handed coordinate system and a depth range of 0 to 1 (D3D style). It allows for quickly creating an orthographic projection matrix by only specifying the left, right, bottom, top, near, and far clipping planes, while using common defaults for the other parameters. The resulting projection matrix can be used in graphics applications to achieve the desired orthographic projection effect when rendering 3D scenes without perspective distortion.
     * @param left The coordinate of the left vertical clipping plane. This defines the minimum x value that will be visible in the rendered scene. Objects with x coordinates less than this value will not be rendered.
     * @param right The coordinate of the right vertical clipping plane. This defines the maximum x value that will be visible in the rendered scene. Objects with x coordinates greater than this value will not be rendered.
     * @param bottom The coordinate of the bottom horizontal clipping plane. This defines the minimum y value that will be visible in the rendered scene. Objects with y coordinates less than this value will not be rendered.
     * @param top The coordinate of the top horizontal clipping plane. This defines the maximum y value that will be visible in the rendered scene. Objects with y coordinates greater than this value will not be rendered.
     * @param z_near The distance to the near clipping plane. Objects closer than this distance will not be rendered. This value must be greater than zero.
     * @param z_far The distance to the far clipping plane. Objects farther than this distance will not be rendered. This value must be greater than the near clipping plane distance.
     * @tparam T Scalar type for the matrix elements and parameters (e.g. float, double).
     * @return A 4x4 orthographic projection matrix that can be used to transform 3D coordinates into clip space for rendering. The resulting matrix is constructed based on the specified left, right, bottom, top, near, and far clipping planes, and uses default values for handedness (right-handed) and depth range (0 to 1). This function provides a convenient way to create an orthographic projection matrix with common defaults, making it easier for developers to set up their projection matrices without needing to specify all parameters explicitly. The orthographic projection is useful for rendering 3D scenes without perspective distortion, where objects maintain their size regardless of their distance from the camera.
     * @note The function assumes that the input parameters are valid (e.g., left < right, bottom < top, positive near and far clipping plane distances) and does not perform error checking on the inputs. It is the responsibility of the caller to ensure that the parameters are within a reasonable range to avoid issues such as division by zero or invalid projection matrices.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> orthographic(
        T left, T right, T bottom, T top, T z_near, T z_far,
        handedness hand = handedness::right,
        depth_range depth = depth_range::zero_to_one) noexcept
    {
        mat<T, 4, 4> m{};

        const T rl = right - left;
        const T tb = top - bottom;

        m(0, 0) = T{2} / rl;
        m(1, 1) = T{2} / tb;
        m(0, 3) = -(right + left) / rl;
        m(1, 3) = -(top + bottom) / tb;

        const T zn = z_near;
        const T zf = z_far;

        if (hand == handedness::left)
        {
            if (depth == depth_range::zero_to_one)
            {
                m(2, 2) = T{1} / (zf - zn);
                m(2, 3) = -zn / (zf - zn);
            }
            else
            {
                m(2, 2) = T{2} / (zf - zn);
                m(2, 3) = -(zf + zn) / (zf - zn);
            }
        }
        else
        {
            if (depth == depth_range::zero_to_one)
            {
                m(2, 2) = T{1} / (zn - zf);
                m(2, 3) = zn / (zn - zf);
            }
            else
            {
                m(2, 2) = T{2} / (zn - zf);
                m(2, 3) = (zf + zn) / (zn - zf);
            }
        }

        m(3, 3) = T{1};
        return m;
    }

} // namespace catalyst::math