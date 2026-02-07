/**
 * @file transform.hpp
 * @brief Functions and types for constructing transformation matrices (translation, rotation, scale) in the Catalyst Math library.
 * @details This header provides functions for creating transformation matrices commonly used in graphics applications, including translation, rotation (from quaternions), and scaling. It also defines a transform struct that encapsulates translation, rotation, and scale components and can be converted to a 4x4 transformation matrix. Additionally, it includes functions for transforming points and vectors using a given transformation matrix, as well as a function for computing the inverse of an affine transformation matrix. These utilities are designed to facilitate common transformation operations in 3D graphics and game development.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include <catalyst/math/mat.hpp>
#include <catalyst/math/quat.hpp>
#include <catalyst/math/vec.hpp>

#include <concepts>
#include <cmath>
#include <cstddef>
#include <optional>
#include <type_traits>

/**
 * @namespace catalyst::math
 * @brief The catalyst::math namespace contains all the mathematical types and functions provided by the Catalyst Math library. This includes vector and matrix types, quaternion support, angle types with user-defined literals, projection matrix utilities, transformation functions, and more. By organizing all math-related functionality within this namespace, we can avoid naming conflicts and provide a clear structure for users of the library to access the various mathematical tools they need for graphics programming and game development.
 * @details The Catalyst Math library is designed to be efficient, easy to use, and compatible with modern C++ standards. It provides a comprehensive set of mathematical utilities that are commonly used in graphics applications, making it easier for developers to perform complex mathematical operations without having to implement them from scratch. By including the appropriate headers from the Catalyst Math library, users can access all the functionality they need for their projects.
 */
namespace catalyst::math
{

    /**
     * @fn translation
     * @brief Creates a translation matrix from the given translation vector. The resulting matrix can be used to apply a translation transformation to points and vectors in 3D space. The translation is specified as a vec<T, 3>, where the x, y, and z components represent the translation along the respective axes. The resulting 4x4 matrix is in column-major order and can be used in graphics APIs that expect this format for transformation matrices.
     * @param t A vec<T, 3> representing the translation along the x, y, and z axes.
     * @tparam T The scalar type for the translation components (e.g. float, double).
     * @return A 4x4 matrix that represents the translation transformation specified by the input vector. The matrix is constructed such that the upper-left 3x3 portion is an identity matrix, and the last column contains the translation components (t.x, t.y, t.z, 1). This allows the resulting matrix to be used directly in homogeneous coordinate transformations commonly used in graphics and game development.
     */
    template <typename T>
        requires is_vec_scalar_v<T>
    [[nodiscard]] constexpr mat<T, 4, 4> translation(const vec<T, 3> &t) noexcept
    {
        mat<T, 4, 4> m = mat<T, 4, 4>::identity();
        m(0, 3) = t.x;
        m(1, 3) = t.y;
        m(2, 3) = t.z;
        return m;
    }

    /**
     * @fn scale
     * @brief Creates a scaling matrix from the given scale vector. The resulting matrix can be used to apply a scaling transformation to points and vectors in 3D space. The scale is specified as a vec<T, 3>, where the x, y, and z components represent the scaling factors along the respective axes. The resulting 4x4 matrix is in column-major order and can be used in graphics APIs that expect this format for transformation matrices.
     * @param s A vec<T, 3> representing the scaling factors along the x, y, and z axes.
     * @tparam T The scalar type for the scaling components (e.g. float, double).
     * @return A 4x4 matrix that represents the scaling transformation specified by the input vector. The matrix is constructed such that the diagonal elements contain the scaling factors (s.x, s.y, s.z) and the last element is 1. This allows the resulting matrix to be used directly in homogeneous coordinate transformations commonly used in graphics and game development.
     */
    template <typename T>
        requires is_vec_scalar_v<T>
    [[nodiscard]] constexpr mat<T, 4, 4> scale(const vec<T, 3> &s) noexcept
    {
        mat<T, 4, 4> m{};
        m(0, 0) = s.x;
        m(1, 1) = s.y;
        m(2, 2) = s.z;
        m(3, 3) = T{1};
        return m;
    }

    /**
     * @fn scale
     * @brief Creates a uniform scaling matrix from the given scalar scale factor. The resulting matrix can be used to apply a uniform scaling transformation to points and vectors in 3D space. The scale is specified as a single scalar value, which is applied uniformly along all three axes (x, y, z). The resulting 4x4 matrix is in column-major order and can be used in graphics APIs that expect this format for transformation matrices.
     * @param s A scalar value representing the uniform scaling factor along all three axes.
     * @tparam T The scalar type for the scaling factor (e.g. float, double).
     * @return A 4x4 matrix that represents the uniform scaling transformation specified by the input scalar. The matrix is constructed such that the diagonal elements contain the scaling factor (s) and the last element is 1. This allows the resulting matrix to be used directly in homogeneous coordinate transformations commonly used in graphics and game development.
     */
    template <typename T>
        requires is_vec_scalar_v<T>
    [[nodiscard]] constexpr mat<T, 4, 4> scale(T s) noexcept
    {
        return scale(vec<T, 3>{s, s, s});
    }

    /**
     * @fn rotation
     * @brief Creates a rotation matrix from the given quaternion. The resulting matrix can be used to apply a rotation transformation to points and vectors in 3D space. The quaternion is expected to represent a valid rotation, and the resulting 4x4 matrix is in column-major order, suitable for use in graphics APIs that expect this format for transformation matrices.
     * @param q A quat<T> representing the rotation to be applied.
     * @tparam T The scalar type for the quaternion components (e.g. float, double).
     * @return A 4x4 matrix that represents the rotation transformation specified by the input quaternion. The function converts the quaternion to a corresponding 4x4 rotation matrix using standard formulas derived from the quaternion components. The resulting matrix can be used directly in homogeneous coordinate transformations commonly used in graphics and game development.
     */
    template <typename T>
        requires is_vec_scalar_v<T>
    [[nodiscard]] inline mat<T, 4, 4> rotation(const quat<T> &q) noexcept
    {
        return q.to_mat4();
    }

    /**
     * @fn trs
     * @brief Creates a combined transformation matrix from the given translation, rotation, and scale components. The resulting matrix can be used to apply a combined transformation to points and vectors in 3D space. The translation is specified as a vec<T, 3>, the rotation is specified as a quat<T>, and the scale is specified as a vec<T, 3>. The resulting 4x4 matrix is in column-major order and can be used in graphics APIs that expect this format for transformation matrices.
     * @param t A vec<T, 3> representing the translation along the x, y, and z axes.
     * @param r A quat<T> representing the rotation to be applied.
     * @param s A vec<T, 3> representing the scaling factors along the x, y, and z axes.
     * @tparam T The scalar type for the translation, rotation, and scale components (e.g. float, double).
     * @return A 4x4 matrix that represents the combined transformation specified by the input translation, rotation, and scale components. The function constructs the combined transformation by multiplying the individual translation, rotation, and scale matrices in the order of translation * rotation * scale. This allows for efficient application of all three transformations in a single matrix multiplication when transforming points or vectors in 3D space.
     */
    template <typename T>
        requires is_vec_scalar_v<T>
    [[nodiscard]] inline mat<T, 4, 4> trs(const vec<T, 3> &t, const quat<T> &r, const vec<T, 3> &s) noexcept
    {
        return translation(t) * rotation(r) * scale(s);
    }

    /**
     * @struct transform
     * @brief A struct that encapsulates translation, rotation, and scale components for a transformation in 3D space. This struct provides a convenient way to represent a transformation using separate components and can be converted to a 4x4 transformation matrix for use in graphics applications. The translation is represented as a vec<T, 3>, the rotation is represented as a quat<T>, and the scale is represented as a vec<T, 3>. The struct includes a method to convert the encapsulated transformation into a 4x4 matrix using the trs function.
     * @tparam T The scalar type for the translation, rotation, and scale components (e.g. float, double).
     */
    template <typename T>
    struct transform
    {
        /**
         * @typedef value_type
         * @brief The scalar type used for the translation, rotation, and scale components of the transform struct. This type is typically a floating-point type such as float or double, and it defines the precision of the transformation components. The value_type can be used for generic programming purposes, allowing functions that operate on transform objects to refer to the underlying scalar type without needing to know the specific type at compile time.
         */
        using value_type = T;

        /**
         * @brief The translation component of the transform, represented as a vec<T, 3>. This vector specifies the translation along the x, y, and z axes that will be applied when the transform is converted to a matrix. The default value is the zero vector (0, 0, 0), which represents no translation.
         */
        vec<T, 3> t{};
        /**
         * @brief The rotation component of the transform, represented as a quat<T>. This quaternion specifies the rotation that will be applied when the transform is converted to a matrix. The default value is the identity quaternion (0, 0, 0, 1), which represents no rotation.
         */
        quat<T> r = quat<T>::identity();
        /**
         * @brief The scale component of the transform, represented as a vec<T, 3>. This vector specifies the scaling factors along the x, y, and z axes that will be applied when the transform is converted to a matrix. The default value is the vector (1, 1, 1), which represents no scaling (i.e., a scale factor of 1 along all axes).
         */
        vec<T, 3> s{T{1}, T{1}, T{1}};
        /**
         * @fn to_mat4
         * @brief Converts this transform to a corresponding 4x4 transformation matrix. The resulting matrix represents the combined effect of the translation, rotation, and scale components encapsulated in this struct. The function uses the trs function to construct the matrix by multiplying the individual translation, rotation, and scale matrices in the order of translation * rotation * scale. This allows for efficient application of all three transformations in a single matrix multiplication when transforming points or vectors in 3D space.
         * @return A 4x4 matrix that represents the combined transformation specified by the translation, rotation, and scale components of this struct. The resulting matrix can be used directly in homogeneous coordinate transformations commonly used in graphics and game development.
         */
        [[nodiscard]] inline mat<T, 4, 4> to_mat4() const noexcept { return trs(t, r, s); }
    };

    /**
     * @fn transform_point
     * @brief Transforms a point in 3D space using the given transformation matrix. The function takes a 4x4 transformation matrix and a point represented as a vec<T, 3>, applies the transformation to the point, and returns the resulting transformed point. The function handles homogeneous coordinates by treating the input point as a 4D vector with a w component of 1, allowing for proper application of translation, rotation, and scaling transformations. If the resulting w component after transformation is not zero, the function performs perspective division to return the correct 3D coordinates of the transformed point.
     * @param m A 4x4 matrix representing the transformation to be applied to the point.
     * @param p A vec<T, 3> representing the point to be transformed.
     * @tparam T The scalar type for the components of the point and the transformation matrix (e.g. float, double).
     * @return A vec<T, 3> representing the transformed point in 3D space. The function first converts the input point to homogeneous coordinates by adding a w component of 1, applies the transformation using matrix multiplication, and then checks the resulting w component. If w is not zero, it performs perspective division to return the correct 3D coordinates. If w is zero, it returns the x, y, and z components directly without division.
     */
    template <typename T>
        requires is_vec_scalar_v<T>
    [[nodiscard]] inline vec<T, 3> transform_point(const mat<T, 4, 4> &m, const vec<T, 3> &p) noexcept
    {
        const vec<T, 4> hp{p.x, p.y, p.z, T{1}};
        const vec<T, 4> out = m * hp;

        if (out.w != T{})
        {
            const T inv_w = T{1} / out.w;
            return vec<T, 3>{out.x * inv_w, out.y * inv_w, out.z * inv_w};
        }

        return vec<T, 3>{out.x, out.y, out.z};
    }

    /**
     * @fn transform_vector
     * @brief Transforms a vector in 3D space using the given transformation matrix. The function takes a 4x4 transformation matrix and a vector represented as a vec<T, 3>, applies the transformation to the vector, and returns the resulting transformed vector. The function handles homogeneous coordinates by treating the input vector as a 4D vector with a w component of 0, which ensures that only the linear transformation (rotation and scaling) is applied to the vector, while translation is ignored. This is appropriate for transforming direction vectors or normals in graphics applications.
     * @param m A 4x4 matrix representing the transformation to be applied to the vector.
     * @param v A vec<T, 3> representing the vector to be transformed.
     * @tparam T The scalar type for the components of the vector and the transformation matrix (e.g. float, double).
     * @return A vec<T, 3> representing the transformed vector in 3D space. The function first converts the input vector to homogeneous coordinates by adding a w component of 0, applies the transformation using matrix multiplication, and then returns the x, y, and z components of the resulting vector. Since the w component is 0, the translation part of the transformation will not affect the output, making this function suitable for transforming direction vectors or normals.
     */
    template <typename T>
        requires is_vec_scalar_v<T>
    [[nodiscard]] inline vec<T, 3> transform_vector(const mat<T, 4, 4> &m, const vec<T, 3> &v) noexcept
    {
        const vec<T, 4> hv{v.x, v.y, v.z, T{0}};
        const vec<T, 4> out = m * hv;
        return vec<T, 3>{out.x, out.y, out.z};
    }

    /**
     * @fn try_inverse_affine
     * @brief Computes the inverse of an affine transformation matrix. The function takes a 4x4 matrix that represents an affine transformation (where the last row is expected to be [0, 0, 0, 1]) and attempts to compute its inverse. If the linear part of the transformation (the upper-left 3x3 portion) is singular (i.e., it does not have an inverse), the function returns std::nullopt to indicate that the inverse cannot be computed. Otherwise, it returns the inverse of the affine transformation as a 4x4 matrix. The function uses the formula for inverting an affine transformation, which involves computing the inverse of the linear part and applying it to the translation component.
     * @param m A 4x4 matrix representing the affine transformation to be inverted. The last row of the matrix is expected to be [0, 0, 0, 1].
     * @tparam T The scalar type for the components of the transformation matrix (e.g. float, double).
     * @return An std::optional containing the inverse of the affine transformation if it exists, or std::nullopt if the linear part of the transformation is singular. If the inverse exists, the returned matrix can be used to apply the inverse transformation to points and vectors in 3D space.
     */
    template <std::floating_point T>
    [[nodiscard]] inline std::optional<mat<T, 4, 4>> try_inverse_affine(const mat<T, 4, 4> &m) noexcept
    {
        const T a00 = m(0, 0), a01 = m(0, 1), a02 = m(0, 2);
        const T a10 = m(1, 0), a11 = m(1, 1), a12 = m(1, 2);
        const T a20 = m(2, 0), a21 = m(2, 1), a22 = m(2, 2);

        const T c00 = (a11 * a22) - (a12 * a21);
        const T c01 = -((a10 * a22) - (a12 * a20));
        const T c02 = (a10 * a21) - (a11 * a20);
        const T c10 = -((a01 * a22) - (a02 * a21));
        const T c11 = (a00 * a22) - (a02 * a20);
        const T c12 = -((a00 * a21) - (a01 * a20));
        const T c20 = (a01 * a12) - (a02 * a11);
        const T c21 = -((a00 * a12) - (a02 * a10));
        const T c22 = (a00 * a11) - (a01 * a10);

        const T det = (a00 * c00) + (a01 * c01) + (a02 * c02);
        if (det == T{})
            return std::nullopt;

        const T inv_det = T{1} / det;

        // inv(A) = adj(A) / det. Build adj(A) from cofactors.
        mat<T, 4, 4> out{};

        // inv(i,j) = C(j,i) / det
        out(0, 0) = c00 * inv_det;
        out(0, 1) = c10 * inv_det;
        out(0, 2) = c20 * inv_det;

        out(1, 0) = c01 * inv_det;
        out(1, 1) = c11 * inv_det;
        out(1, 2) = c21 * inv_det;

        out(2, 0) = c02 * inv_det;
        out(2, 1) = c12 * inv_det;
        out(2, 2) = c22 * inv_det;

        // Translation: -inv(A) * t
        const vec<T, 3> t{m(0, 3), m(1, 3), m(2, 3)};
        const vec<T, 3> inv_t{
            -(out(0, 0) * t.x + out(0, 1) * t.y + out(0, 2) * t.z),
            -(out(1, 0) * t.x + out(1, 1) * t.y + out(1, 2) * t.z),
            -(out(2, 0) * t.x + out(2, 1) * t.y + out(2, 2) * t.z),
        };

        out(0, 3) = inv_t.x;
        out(1, 3) = inv_t.y;
        out(2, 3) = inv_t.z;
        out(3, 3) = T{1};

        return out;
    }

    /**
     * @fn inverse_affine
     * @brief Computes the inverse of an affine transformation matrix. This function is a wrapper around try_inverse_affine that returns the inverse matrix directly if it exists, or a default-constructed matrix (which is typically the zero matrix) if the inverse cannot be computed due to a singular linear part. The function is designed for convenience when the caller expects that the inverse should exist and does not want to handle the case of a singular matrix separately.
     * @param m A 4x4 matrix representing the affine transformation to be inverted. The last row of the matrix is expected to be [0, 0, 0, 1].
     * @tparam T The scalar type for the components of the transformation matrix (e.g. float, double).
     * @return A 4x4 matrix that represents the inverse of the affine transformation if it exists, or a default-constructed matrix if the linear part of the transformation is singular. If the inverse exists, the returned matrix can be used to apply the inverse transformation to points and vectors in 3D space.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 4, 4> inverse_affine(const mat<T, 4, 4> &m) noexcept
    {
        if (auto inv = try_inverse_affine(m))
            return *inv;
        return mat<T, 4, 4>{};
    }

} // namespace catalyst::math
