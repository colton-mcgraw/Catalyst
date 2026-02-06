/**
 * @file vec.hpp
 * @brief A simple fixed-size vector template with basic arithmetic operations and utilities for common vector functions.
 * @details The vec template is designed for small fixed-size vectors commonly used in graphics applications (e.g. 2D, 3D, 4D vectors). It provides basic arithmetic operations (addition, subtraction, scalar multiplication/division) and utilities for accessing components and loading/storing from arrays. The vector is stored as a simple array of components, and the template parameters allow for flexibility in the size and alignment of the vector.
 * License: MIT (see LICENSE).
 */

#pragma once

#include <catalyst/math/simd.hpp>
#include <catalyst/math/detail/simd_array_ops.hpp>

#include <array>
#include <algorithm>
#include <concepts>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <type_traits>

/**
 * @namespace catalyst::math
 * @brief The catalyst::math namespace contains all the mathematical types and functions provided by the Catalyst Math library. This includes vector and matrix types, quaternion support, angle types with user-defined literals, projection matrix utilities, transformation functions, and more. By organizing all math-related functionality within this namespace, we can avoid naming conflicts and provide a clear structure for users of the library to access the various mathematical tools they need for graphics programming and game development.
 * @details The Catalyst Math library is designed to be efficient, easy to use, and compatible with modern C++ standards. It provides a comprehensive set of mathematical utilities that are commonly used in graphics applications, making it easier for developers to perform complex mathematical operations without having to implement them from scratch. By including the appropriate headers from the Catalyst Math library, users can access all the functionality they need for their projects.
 */
namespace catalyst::math
{

    /**
     * @typedef is_vec_scalar_v
     * @brief A type trait that checks if a given type T is a valid scalar type for vector components. This trait is used to constrain the template parameters of the vec template to ensure that only arithmetic types (e.g. int, float, double) can be used as the scalar type for vector components. The trait evaluates to true if T is an arithmetic type, and false otherwise.
     * @tparam T The type to be checked.
     */
    template <typename T>
    inline constexpr bool is_vec_scalar_v = std::is_arithmetic_v<T>;

    /**
     * @struct vec
     * @tparam T Scalar type for the vector components (e.g. float, double).
     * @tparam N The number of components in the vector (e.g. 2 for vec2, 3 for vec3, 4 for vec4).
     * @tparam Align The alignment requirement for the vector type (default is alignof(T)).
     * @tparam Enable A template parameter used for SFINAE to enable or disable the vec template based on the validity of the scalar type T.
     * @brief A simple fixed-size vector template with basic arithmetic operations and utilities for common vector functions. The vec template is designed for small fixed-size vectors commonly used in graphics applications (e.g. 2D, 3D, 4D vectors). It provides basic arithmetic operations (addition, subtraction, scalar multiplication/division) and utilities for accessing components and loading/storing from arrays. The vector is stored as a simple array of components, and the template parameters allow for flexibility in the size and alignment of the vector.
     * @details The vec template is designed for small fixed-size vectors commonly used in graphics applications (e.g. 2D, 3D, 4D vectors). It provides basic arithmetic operations (addition, subtraction, scalar multiplication/division) and utilities for accessing components and loading/storing from arrays. The vector is stored as a simple array of components, and the template parameters allow for flexibility in the size and alignment of the vector. The template is constrained using SFINAE to ensure that only valid scalar types can be used for the vector components. This design allows for efficient and convenient use of vectors in graphics programming and game development, while also providing the necessary type safety and flexibility.
     */
    template <typename T, std::size_t N, std::size_t Align, typename Enable>
    struct vec;

    namespace detail
    {
        /**
         * @typedef swizzle_vec_t
         * @brief A type alias for a vector type used in swizzling operations. This alias defines a vec type with the same scalar type T and size M, but with an alignment equal to alignof(T). The swizzle_vec_t is used internally for implementing swizzling operations on vectors, allowing for flexible access to components in different orders while maintaining the same underlying data type and size.
         * @tparam T The scalar type for the vector components (e.g. float, double).
         * @tparam M The number of components in the swizzle vector (e.g. 2 for vec2, 3 for vec3, 4 for vec4).
         */
        template <typename T, std::size_t M>
        using swizzle_vec_t = ::catalyst::math::vec<T, M, alignof(T), std::enable_if_t<is_vec_scalar_v<T>>>;

        /**
         * @struct vec_common
         * @tparam Derived The derived vector type that inherits from vec_common (e.g. vec2, vec3, vec4).
         * @tparam T Scalar type for the vector components (e.g. float, double).
         * @tparam N The number of components in the vector (e.g. 2 for vec2, 3 for vec3, 4 for vec4).
         * @brief A common base struct for vector types that provides basic arithmetic operations and utilities for vectors. The vec_common struct is designed to be inherited by specific vector types (e.g. vec2, vec3, vec4) and provides implementations for common operations such as addition, subtraction, scalar multiplication/division, dot product, length calculation, and component access. The use of the CRTP (Curiously Recurring Template Pattern) allows for static polymorphism and efficient code reuse across different vector types while maintaining type safety and flexibility.
         * @details The vec_common struct is designed to be inherited by specific vector types (e.g. vec2, vec3, vec4) and provides implementations for common operations such as addition, subtraction, scalar multiplication/division, dot product, length calculation, and component access. The use of the CRTP (Curiously Recurring Template Pattern) allows for static polymorphism and efficient code reuse across different vector types while maintaining type safety and flexibility. This design enables the creation of various vector types with different sizes and scalar types while sharing a common set of operations and utilities.
         */
        template <typename Derived, typename T, std::size_t N>
        struct vec_common
        {
            /**
             * @brief The scalar type used for the vector components (e.g. float, double).
             */
            using value_type = T;
            /**
             * @brief The number of components in the vector (e.g. 2 for vec2, 3 for vec3, 4 for vec4).
             */
            static constexpr std::size_t size = N;

            /**
             * @fn data
             * @brief Returns a pointer to the underlying array of components in the vector. This function provides access to the raw data of the vector, allowing for operations that require direct access to the components, such as loading from or storing to arrays. The non-const version returns a pointer to the modifiable data, while the const version returns a pointer to the read-only data.
             * @return A pointer to the underlying array of components in the vector. The returned pointer can be used for operations that require direct access to the components, such as loading from or storing to arrays. The non-const version allows for modification of the vector components, while the const version provides read-only access.
             */
            [[nodiscard]] constexpr T *data() noexcept { return static_cast<Derived *>(this)->data(); }
            /**
             * @fn data
             * @brief Returns a pointer to the underlying array of components in the vector. This function provides access to the raw data of the vector, allowing for operations that require direct access to the components, such as loading from or storing to arrays. The non-const version returns a pointer to the modifiable data, while the const version returns a pointer to the read-only data.
             * @return A pointer to the underlying array of components in the vector. The returned pointer can be used for operations that require direct access to the components, such as loading from or storing to arrays. The non-const version allows for modification of the vector components, while the const version provides read-only access.
             */
            [[nodiscard]] constexpr const T *data() const noexcept { return static_cast<const Derived *>(this)->data(); }

            /**
             * @fn operator[]
             * @brief Provides access to the components of the vector using array indexing syntax. The non-const version allows for modification of the vector components, while the const version provides read-only access. The operator[] function does not perform bounds checking, so it is the responsibility of the caller to ensure that the index is within the valid range (0 to N-1).
             * @param i The index of the component to access (0-based). The caller must ensure that the index is within the valid range (0 to N-1) to avoid undefined behavior.
             * @return A reference to the component at the specified index. The non-const version allows for modification of the vector component, while the const version provides read-only access. The operator[] function does not perform bounds checking, so it is the responsibility of the caller to ensure that the index is within the valid range (0 to N-1).
             */
            [[nodiscard]] constexpr T &operator[](std::size_t i) noexcept { return data()[i]; }
            /**
             * @fn operator[]
             * @brief Provides access to the components of the vector using array indexing syntax. The non-const version allows for modification of the vector components, while the const version provides read-only access. The operator[] function does not perform bounds checking, so it is the responsibility of the caller to ensure that the index is within the valid range (0 to N-1).
             * @param i The index of the component to access (0-based). The caller must ensure that the index is within the valid range (0 to N-1) to avoid undefined behavior.
             * @return A reference to the component at the specified index. The non-const version allows for modification of the vector component, while the const version provides read-only access. The operator[] function does not perform bounds checking, so it is the responsibility of the caller to ensure that the index is within the valid range (0 to N-1).
             */
            [[nodiscard]] constexpr const T &operator[](std::size_t i) const noexcept { return data()[i]; }

            /**
             * @fn load
             * @brief Loads vector components from an array of scalars. This static member function takes a pointer to an array of scalars and constructs a vector by copying the values from the array into the vector's components. The caller must ensure that the input array has at least N elements to avoid undefined behavior.
             * @param ptr A pointer to an array of scalars from which to load the vector components. The caller must ensure that the input array has at least N elements to avoid undefined behavior.
             * @return A vector constructed by copying the values from the input array into its components. The function creates a new vector instance, copies the values from the input array, and returns it. The caller can use this function to initialize a vector from an existing array of scalar values.
             */
            [[nodiscard]] static constexpr Derived load(const T *ptr) noexcept
            {
                Derived out;
                detail::copy<T, N>(out.data(), ptr);
                return out;
            }

            /**
             * @fn store
             * @brief Stores the vector components into an array of scalars. This member function takes a pointer to an array of scalars and copies the values from the vector's components into the array. The caller must ensure that the output array has at least N elements to avoid undefined behavior.
             * @param ptr A pointer to an array of scalars where the vector components will be stored. The caller must ensure that the output array has at least N elements to avoid undefined behavior.
             * @return void. The function copies the values from the vector's components into the provided output array. The caller can use this function to extract the components of a vector into an existing array for further processing or storage.
             */
            constexpr void store(T *ptr) const noexcept
            {
                detail::copy<T, N>(ptr, data());
            }
            /**
             * @fn store
             * @brief Stores the vector components into an array of scalars. This member function takes a pointer to an array of scalars and copies the values from the vector's components into the array. The caller must ensure that the output array has at least N elements to avoid undefined behavior.
             * @param ptr A pointer to an array of scalars where the vector components will be stored. The caller must ensure that the output array has at least N elements to avoid undefined behavior.
             * @return void. The function copies the values from the vector's components into the provided output array. The caller can use this function to extract the components of a vector into an existing array for further processing or storage.
             */
            [[nodiscard]] constexpr Derived operator+() const noexcept { return *static_cast<const Derived *>(this); }
            /**
             * @fn operator-
             * @brief Returns the negation of the vector. This unary operator creates a new vector where each component is the negation of the corresponding component in the original vector. The resulting vector points in the opposite direction in the vector space, and can be useful for operations such as reversing a direction or applying an inverse transformation.
             * @return A new vector that is the negation of the original vector. Each component of the resulting vector is the negation of the corresponding component in the original vector. The function creates a new instance of the derived vector type, negates each component, and returns it.
             */
            [[nodiscard]] constexpr Derived operator-() const noexcept
            {
                Derived out;
                for (std::size_t i = 0; i < N; ++i)
                    out.data()[i] = -data()[i];
                return out;
            }
            /**
             * @fn operator+
             * @brief Adds two vectors component-wise. This binary operator takes two vectors of the same type and returns a new vector where each component is the sum of the corresponding components from the input vectors. The resulting vector represents the combined effect of the two input vectors in the vector space, and can be used for operations such as combining translations, velocities, or forces.
             * @param a The first vector operand in the addition operation.
             * @param b The second vector operand in the addition operation. Both vectors must be of the same type (i.e., have the same scalar type and number of components) for this operator to be valid.
             * @return A new vector that is the component-wise sum of the two input vectors. Each component of the resulting vector is calculated as a[i] + b[i], where a[i] and b[i] are the corresponding components of the input vectors. The function creates a new instance of the derived vector type, performs the addition for each component, and returns it.
             */
            [[nodiscard]] friend constexpr Derived operator+(Derived a, const Derived &b) noexcept
            {
                detail::add_inplace<T, N>(a.data(), b.data());
                return a;
            }
            /**
             * @fn operator-
             * @brief Subtracts two vectors component-wise. This binary operator takes two vectors of the same type and returns a new vector where each component is the difference of the corresponding components from the input vectors. The resulting vector represents the relative difference between the two input vectors in the vector space, and can be used for operations such as calculating displacement, velocity change, or force difference.
             * @param a The first vector operand in the subtraction operation.
             * @param b The second vector operand in the subtraction operation. Both vectors must be of the same type (i.e., have the same scalar type and number of components) for this operator to be valid.
             * @return A new vector that is the component-wise difference of the two input vectors. Each component of the resulting vector is calculated as a[i] - b[i], where a[i] and b[i] are the corresponding components of the input vectors. The function creates a new instance of the derived vector type, performs the subtraction for each component, and returns it.
             */
            [[nodiscard]] friend constexpr Derived operator-(Derived a, const Derived &b) noexcept
            {
                detail::sub_inplace<T, N>(a.data(), b.data());
                return a;
            }
            /**
             * @fn operator*
             * @brief Multiplies two vectors component-wise. This binary operator takes two vectors of the same type and returns a new vector where each component is the product of the corresponding components from the input vectors. The resulting vector represents the combined effect of the two input vectors in terms of scaling, and can be used for operations such as applying non-uniform scaling or combining effects in a multiplicative manner.
             * @param a The first vector operand in the multiplication operation.
             * @param b The second vector operand in the multiplication operation. Both vectors must be of the same type (i.e., have the same scalar type and number of components) for this operator to be valid.
             * @return A new vector that is the component-wise product of the two input vectors. Each component of the resulting vector is calculated as a[i] * b[i], where a[i] and b[i] are the corresponding components of the input vectors. The function creates a new instance of the derived vector type, performs the multiplication for each component, and returns it.
             */
            [[nodiscard]] friend constexpr Derived operator*(Derived a, const Derived &b) noexcept
            {
                detail::mul_inplace<T, N>(a.data(), b.data());
                return a;
            }
            /**
             * @fn operator/
             * @brief Divides two vectors component-wise. This binary operator takes two vectors of the same type and returns a new vector where each component is the quotient of the corresponding components from the input vectors. The resulting vector represents the relative scaling between the two input vectors in the vector space, and can be used for operations such as applying non-uniform inverse scaling or combining effects in a divisive manner.
             * @param a The first vector operand in the division operation.
             * @param b The second vector operand in the division operation. Both vectors must be of the same type (i.e., have the same scalar type and number of components) for this operator to be valid.
             * @return A new vector that is the component-wise quotient of the two input vectors. Each component of the resulting vector is calculated as a[i] / b[i], where a[i] and b[i] are the corresponding components of the input vectors. The function creates a new instance of the derived vector type, performs the division for each component, and returns it. The caller must ensure that none of the components of b are zero to avoid undefined behavior due to division by zero.
             */
            [[nodiscard]] friend constexpr Derived operator/(Derived a, const Derived &b) noexcept
            {
                detail::div_inplace<T, N>(a.data(), b.data());
                return a;
            }
            /**
             * @fn operator+
             * @brief Adds a scalar value to each component of the vector. This binary operator takes a vector and a scalar value, and returns a new vector where each component is the sum of the corresponding component from the input vector and the scalar value. The resulting vector represents the effect of adding a constant value to each component of the original vector, which can be useful for operations such as translating a position or adjusting a color.
             * @param a The vector operand in the addition operation.
             * @param s The scalar value to be added to each component of the vector. The scalar must be of the same type as the components of the vector for this operator to be valid.
             * @return A new vector that is the result of adding the scalar value to each component of the input vector. Each component of the resulting vector is calculated as a[i] + s, where a[i] is the corresponding component of the input vector and s is the scalar value. The function creates a new instance of the derived vector type, performs the addition for each component, and returns it.
             */
            [[nodiscard]] friend constexpr Derived operator+(Derived a, T s) noexcept
            {
                for (std::size_t i = 0; i < N; ++i)
                    a.data()[i] += s;
                return a;
            }
            /**
             * @fn operator-
             * @brief Subtracts a scalar value from each component of the vector. This binary operator takes a vector and a scalar value, and returns a new vector where each component is the difference of the corresponding component from the input vector and the scalar value. The resulting vector represents the effect of subtracting a constant value from each component of the original vector, which can be useful for operations such as translating a position in the opposite direction or adjusting a color by reducing its intensity.
             * @param a The vector operand in the subtraction operation.
             * @param s The scalar value to be subtracted from each component of the vector. The scalar must be of the same type as the components of the vector for this operator to be valid.
             * @return A new vector that is the result of subtracting the scalar value from each component of the input vector. Each component of the resulting vector is calculated as a[i] - s, where a[i] is the corresponding component of the input vector and s is the scalar value. The function creates a new instance of the derived vector type, performs the subtraction for each component, and returns it.
             */
            [[nodiscard]] friend constexpr Derived operator-(Derived a, T s) noexcept
            {
                for (std::size_t i = 0; i < N; ++i)
                    a.data()[i] -= s;
                return a;
            }
            /**
             * @fn operator*
             * @brief Multiplies each component of the vector by a scalar value. This binary operator takes a vector and a scalar value, and returns a new vector where each component is the product of the corresponding component from the input vector and the scalar value. The resulting vector represents the effect of scaling the original vector by a constant factor, which can be useful for operations such as resizing an object or adjusting the intensity of a color.
             * @param a The vector operand in the multiplication operation.
             * @param s The scalar value to be multiplied with each component of the vector. The scalar must be of the same type as the components of the vector for this operator to be valid.
             * @return A new vector that is the result of multiplying each component of the input vector by the scalar value. Each component of the resulting vector is calculated as a[i] * s, where a[i] is the corresponding component of the input vector and s is the scalar value. The function creates a new instance of the derived vector type, performs the multiplication for each component, and returns it.
             */
            [[nodiscard]] friend constexpr Derived operator*(Derived a, T s) noexcept
            {
                for (std::size_t i = 0; i < N; ++i)
                    a.data()[i] *= s;
                return a;
            }
            /**
             * @fn operator/
             * @brief Divides each component of the vector by a scalar value. This binary operator takes a vector and a scalar value, and returns a new vector where each component is the quotient of the corresponding component from the input vector and the scalar value. The resulting vector represents the effect of scaling the original vector by the inverse of a constant factor, which can be useful for operations such as resizing an object by a fractional amount or adjusting the intensity of a color by reducing it.
             * @param a The vector operand in the division operation.
             * @param s The scalar value to be divided with each component of the vector. The scalar must be of the same type as the components of the vector for this operator to be valid.
             * @return A new vector that is the result of dividing each component of the input vector by the scalar value. Each component of the resulting vector is calculated as a[i] / s, where a[i] is the corresponding component of the input vector and s is the scalar value. The function creates a new instance of the derived vector type, performs the division for each component, and returns it. The caller must ensure that s is not zero to avoid undefined behavior due to division by zero.
             */
            [[nodiscard]] friend constexpr Derived operator/(Derived a, T s) noexcept
            {
                for (std::size_t i = 0; i < N; ++i)
                    a.data()[i] /= s;
                return a;
            }
            /**
             * @fn operator+
             * @brief Adds a scalar value to each component of the vector. This binary operator takes a scalar value and a vector, and returns a new vector where each component is the sum of the scalar value and the corresponding component from the input vector. The resulting vector represents the effect of adding a constant value to each component of the original vector, which can be useful for operations such as translating a position or adjusting a color.
             * @param s The scalar value to be added to each component of the vector. The scalar must be of the same type as the components of the vector for this operator to be valid.
             * @param a The vector operand in the addition operation.
             * @return A new vector that is the result of adding the scalar value to each component of the input vector. Each component of the resulting vector is calculated as s + a[i], where s is the scalar value and a[i] is the corresponding component of the input vector. The function creates a new instance of the derived vector type, performs the addition for each component, and returns it.
             */
            [[nodiscard]] friend constexpr Derived operator+(T s, Derived a) noexcept { return a + s; }
            /**
             * @fn operator-
             * @brief Subtracts a scalar value from each component of the vector. This binary operator takes a scalar value and a vector, and returns a new vector where each component is the difference of the scalar value and the corresponding component from the input vector. The resulting vector represents the effect of subtracting each component of the original vector from a constant value, which can be useful for operations such as translating a position in the opposite direction or adjusting a color by reducing its intensity.
             * @param s The scalar value to be subtracted from each component of the vector. The scalar must be of the same type as the components of the vector for this operator to be valid.
             * @param a The vector operand in the subtraction operation.
             * @return A new vector that is the result of subtracting each component of the input vector from the scalar value. Each component of the resulting vector is calculated as s - a[i], where s is the scalar value and a[i] is the corresponding component of the input vector. The function creates a new instance of the derived vector type, performs the subtraction for each component, and returns it.
             */
            [[nodiscard]] friend constexpr Derived operator-(T s, Derived a) noexcept
            {
                Derived out;
                for (std::size_t i = 0; i < N; ++i)
                    out.data()[i] = s - a.data()[i];
                return out;
            }
            /**
             * @fn operator*
             * @brief Multiplies each component of the vector by a scalar value. This binary operator takes a scalar value and a vector, and returns a new vector where each component is the product of the scalar value and the corresponding component from the input vector. The resulting vector represents the effect of scaling the original vector by a constant factor, which can be useful for operations such as resizing an object or adjusting the intensity of a color.
             * @param s The scalar value to be multiplied with each component of the vector. The scalar must be of the same type as the components of the vector for this operator to be valid.
             * @param a The vector operand in the multiplication operation.
             * @return A new vector that is the result of multiplying each component of the input vector by the scalar value. Each component of the resulting vector is calculated as s * a[i], where s is the scalar value and a[i] is the corresponding component of the input vector. The function creates a new instance of the derived vector type, performs the multiplication for each component, and returns it.
             */
            [[nodiscard]] friend constexpr Derived operator*(T s, Derived a) noexcept { return a * s; }
            /**
             * @fn operator/
             * @brief Divides each component of the vector by a scalar value. This binary operator takes a scalar value and a vector, and returns a new vector where each component is the quotient of the scalar value and the corresponding component from the input vector. The resulting vector represents the effect of scaling the original vector by the inverse of a constant factor, which can be useful for operations such as resizing an object by a fractional amount or adjusting the intensity of a color by reducing it.
             * @param s The scalar value to be divided with each component of the vector. The scalar must be of the same type as the components of the vector for this operator to be valid.
             * @param a The vector operand in the division operation.
             * @return A new vector that is the result of dividing each component of the input vector by the scalar value. Each component of the resulting vector is calculated as s / a[i], where s is the scalar value and a[i] is the corresponding component of the input vector. The function creates a new instance of the derived vector type, performs the division for each component, and returns it. The caller must ensure that none of the components of a are zero to avoid undefined behavior due to division by zero.
             */
            [[nodiscard]] friend constexpr Derived operator/(T s, Derived a) noexcept
            {
                Derived out;
                for (std::size_t i = 0; i < N; ++i)
                    out.data()[i] = s / a.data()[i];
                return out;
            }

            /**
             * @fn operator+=
             * @brief Adds another vector to this vector component-wise and assigns the result to this vector. This compound assignment operator takes another vector of the same type and adds its components to the corresponding components of this vector, updating this vector with the result. The resulting vector represents the combined effect of this vector and the other vector in the vector space, and can be used for operations such as combining translations, velocities, or forces.
             * @param other The other vector to be added to this vector. The other vector must be of the same type (i.e., have the same scalar type and number of components) for this operator to be valid.
             * @return A reference to this vector after it has been updated with the component-wise sum of itself and the other vector. Each component of this vector is updated as data()[i] += other.data()[i], where data()[i] is the corresponding component of this vector and other.data()[i] is the corresponding component of the other vector. The function updates this vector in place and returns a reference to it.
             */
            constexpr Derived &operator+=(const Derived &other) noexcept { return *static_cast<Derived *>(this) = (*static_cast<Derived *>(this) + other); }
            /**
             * @fn operator-=
             * @brief Subtracts another vector from this vector component-wise and assigns the result to this vector. This compound assignment operator takes another vector of the same type and subtracts its components from the corresponding components of this vector, updating this vector with the result. The resulting vector represents the relative difference between this vector and the other vector in the vector space, and can be used for operations such as calculating displacement, velocity change, or force difference.
             * @param other The other vector to be subtracted from this vector. The other vector must be of the same type (i.e., have the same scalar type and number of components) for this operator to be valid.
             * @return A reference to this vector after it has been updated with the component-wise difference of itself and the other vector. Each component of this vector is updated as data()[i] -= other.data()[i], where data()[i] is the corresponding component of this vector and other.data()[i] is the corresponding component of the other vector. The function updates this vector in place and returns a reference to it.
             */
            constexpr Derived &operator-=(const Derived &other) noexcept { return *static_cast<Derived *>(this) = (*static_cast<Derived *>(this) - other); }
            /**
             * @fn operator*=
             * @brief Multiplies this vector by another vector component-wise and assigns the result to this vector. This compound assignment operator takes another vector of the same type and multiplies its components with the corresponding components of this vector, updating this vector with the result. The resulting vector represents the combined effect of this vector and the other vector in terms of scaling, and can be used for operations such as applying non-uniform scaling or combining effects in a multiplicative manner.
             * @param other The other vector to be multiplied with this vector. The other vector must be of the same type (i.e., have the same scalar type and number of components) for this operator to be valid.
             * @return A reference to this vector after it has been updated with the component-wise product of itself and the other vector. Each component of this vector is updated as data()[i] *= other.data()[i], where data()[i] is the corresponding component of this vector and other.data()[i] is the corresponding component of the other vector. The function updates this vector in place and returns a reference to it.
             */
            constexpr Derived &operator*=(const Derived &other) noexcept { return *static_cast<Derived *>(this) = (*static_cast<Derived *>(this) * other); }
            /**
             * @fn operator/=
             * @brief Divides this vector by another vector component-wise and assigns the result to this vector. This compound assignment operator takes another vector of the same type and divides its components with the corresponding components of this vector, updating this vector with the result. The resulting vector represents the relative scaling between this vector and the other vector in the vector space, and can be used for operations such as applying non-uniform inverse scaling or combining effects in a divisive manner.
             * @param other The other vector to be divided with this vector. The other vector must be of the same type (i.e., have the same scalar type and number of components) for this operator to be valid.
             * @return A reference to this vector after it has been updated with the component-wise quotient of itself and the other vector. Each component of this vector is updated as data()[i] /= other.data()[i], where data()[i] is the corresponding component of this vector and other.data()[i] is the corresponding component of the other vector. The function updates this vector in place and returns a reference to it. The caller must ensure that none of the components of other are zero to avoid undefined behavior due to division by zero.
             */
            constexpr Derived &operator/=(const Derived &other) noexcept { return *static_cast<Derived *>(this) = (*static_cast<Derived *>(this) / other); }

            /**
             * @fn operator+=
             * @brief Adds a scalar value to each component of this vector and assigns the result to this vector. This compound assignment operator takes a scalar value and adds it to each component of this vector, updating this vector with the result. The resulting vector represents the effect of adding a constant value to each component of the original vector, which can be useful for operations such as translating a position or adjusting a color.
             * @param s The scalar value to be added to each component of this vector. The scalar must be of the same type as the components of the vector for this operator to be valid.
             * @return A reference to this vector after it has been updated with the result of adding the scalar value to each component. Each component of this vector is updated as data()[i] += s, where data()[i] is the corresponding component of this vector and s is the scalar value. The function updates this vector in place and returns a reference to it.
             */
            constexpr Derived &operator+=(T s) noexcept { return *static_cast<Derived *>(this) = (*static_cast<Derived *>(this) + s); }
            /**
             * @fn operator-=
             * @brief Subtracts a scalar value from each component of this vector and assigns the result to this vector. This compound assignment operator takes a scalar value and subtracts it from each component of this vector, updating this vector with the result. The resulting vector represents the effect of subtracting a constant value from each component of the original vector, which can be useful for operations such as translating a position in the opposite direction or adjusting a color by reducing its intensity.
             * @param s The scalar value to be subtracted from each component of this vector. The scalar must be of the same type as the components of the vector for this operator to be valid.
             * @return A reference to this vector after it has been updated with the result of subtracting the scalar value from each component. Each component of this vector is updated as data()[i] -= s, where data()[i] is the corresponding component of this vector and s is the scalar value. The function updates this vector in place and returns a reference to it.
             */
            constexpr Derived &operator-=(T s) noexcept { return *static_cast<Derived *>(this) = (*static_cast<Derived *>(this) - s); }
            /**
             * @fn operator*=
             * @brief Multiplies each component of this vector by a scalar value and assigns the result to this vector. This compound assignment operator takes a scalar value and multiplies it with each component of this vector, updating this vector with the result. The resulting vector represents the effect of scaling the original vector by a constant factor, which can be useful for operations such as resizing an object or adjusting the intensity of a color.
             * @param s The scalar value to be multiplied with each component of this vector. The scalar must be of the same type as the components of the vector for this operator to be valid.
             * @return A reference to this vector after it has been updated with the result of multiplying each component by the scalar value. Each component of this vector is updated as data()[i] *= s, where data()[i] is the corresponding component of this vector and s is the scalar value. The function updates this vector in place and returns a reference to it.
             */
            constexpr Derived &operator*=(T s) noexcept { return *static_cast<Derived *>(this) = (*static_cast<Derived *>(this) * s); }
            /**
             * @fn operator/=
             * @brief Divides each component of this vector by a scalar value and assigns the result to this vector. This compound assignment operator takes a scalar value and divides each component of this vector by it, updating this vector with the result. The resulting vector represents the effect of scaling the original vector by the inverse of a constant factor, which can be useful for operations such as resizing an object by a fractional amount or adjusting the intensity of a color by reducing it.
             * @param s The scalar value to be divided with each component of this vector. The scalar must be of the same type as the components of the vector for this operator to be valid.
             * @return A reference to this vector after it has been updated with the result of dividing each component by the scalar value. Each component of this vector is updated as data()[i] /= s, where data()[i] is the corresponding component of this vector and s is the scalar value. The function updates this vector in place and returns a reference to it. The caller must ensure that s is not zero to avoid undefined behavior due to division by zero.
             */
            constexpr Derived &operator/=(T s) noexcept { return *static_cast<Derived *>(this) = (*static_cast<Derived *>(this) / s); }

            /**
             * @fn operator==
             * @brief Compares two vectors for equality. This binary operator takes two vectors of the same type and returns true if all corresponding components of the vectors are equal, and false otherwise. The resulting boolean value indicates whether the two vectors represent the same point or direction in the vector space, which can be useful for operations such as checking for convergence, testing conditions, or implementing equality semantics.
             * @param a The first vector operand in the equality comparison.
             * @param b The second vector operand in the equality comparison. Both vectors must be of the same type (i.e., have the same scalar type and number of components) for this operator to be valid.
             * @return true if all corresponding components of the two input vectors are equal, and false otherwise. The function compares each component of vector a with the corresponding component of vector b using the equality operator (==), and returns true only if all comparisons return true. If any pair of corresponding components are not equal, the function returns false.
             */
            [[nodiscard]] friend constexpr bool operator==(const Derived &a, const Derived &b) noexcept
            {
                for (std::size_t i = 0; i < N; ++i)
                {
                    if (!(a.data()[i] == b.data()[i]))
                        return false;
                }
                return true;
            }

            /**
             * @fn operator!=
             * @brief Compares two vectors for inequality. This binary operator takes two vectors of the same type and returns true if any corresponding components of the vectors are not equal, and false if all components are equal. The resulting boolean value indicates whether the two vectors represent different points or directions in the vector space, which can be useful for operations such as checking for divergence, testing conditions, or implementing inequality semantics.
             * @param a The first vector operand in the inequality comparison.
             * @param b The second vector operand in the inequality comparison. Both vectors must be of the same type (i.e., have the same scalar type and number of components) for this operator to be valid.
             * @return true if any corresponding components of the two input vectors are not equal, and false if all components are equal. The function compares each component of vector a with the corresponding component of vector b using the equality operator (==), and returns false only if all comparisons return true. If any pair of corresponding components are not equal, the function returns true.
             */
            [[nodiscard]] friend constexpr bool operator!=(const Derived &a, const Derived &b) noexcept { return !(a == b); }

            /**
             * @fn dot
             * @brief Computes the dot product of this vector with another vector. The dot product is a scalar value that represents the magnitude of the projection of one vector onto another, and can be used for operations such as calculating angles between vectors, determining orthogonality, or applying lighting models in graphics.
             * @param other The other vector to compute the dot product with. The other vector must be of the same type (i.e., have the same scalar type and number of components) for this function to be valid.
             * @return The dot product of this vector and the other vector. The result is calculated as the sum of the products of corresponding components from the two vectors, i.e., data()[0] * other.data()[0] + data()[1] * other.data()[1] + ... + data()[N-1] * other.data()[N-1]. The function returns a scalar value of type T that represents the dot product.
             */
            [[nodiscard]] constexpr T dot(const Derived &other) const noexcept
            {
                return detail::dot<T, N>(data(), other.data());
            }

            /**
             * @fn length_sq
             * @brief Computes the squared length (magnitude) of this vector. The squared length is the sum of the squares of the components of the vector, and can be used for operations where the actual length is not required, such as comparing lengths or optimizing performance by avoiding a square root calculation.
             * @tparam U An optional template parameter that specifies the return type of the squared length. This allows the function to return a different type than the scalar type of the vector components, which can be useful for precision or performance reasons. The default type is T, which is the scalar type of the vector components.
             * @return The squared length of this vector. The result is calculated as data()[0] * data()[0] + data()[1] * data()[1] + ... + data()[N-1] * data()[N-1]. The function returns a scalar value of type T that represents the squared length.
             */
            template <typename U = T>
                requires std::floating_point<U>
            [[nodiscard]] U length_sq() const noexcept
            {
                return static_cast<U>(this->dot(*static_cast<const Derived *>(this)));
            }

            /**
             * @fn length
             * @brief Computes the length (magnitude) of this vector. The length is the square root of the sum of the squares of the components of the vector, and represents the distance from the origin to the point represented by the vector in the vector space. It can be used for operations such as normalizing a vector, calculating distances, or determining the size of an object.
             * @tparam U An optional template parameter that specifies the return type of the length. This allows the function to return a different type than the scalar type of the vector components, which can be useful for precision or performance reasons. The default type is T, which is the scalar type of the vector components.
             * @return The length of this vector. The result is calculated as the square root of (data()[0] * data()[0] + data()[1] * data()[1] + ... + data()[N-1] * data()[N-1]). The function returns a scalar value of type U that represents the length.
             */
            template <typename U = T>
                requires std::floating_point<U>
            [[nodiscard]] U length() const noexcept
            {
                return static_cast<U>(std::sqrt(this->length_sq()));
            }

            /**
             * @fn normalized
             * @brief Returns a normalized (unit length) version of this vector. A normalized vector has the same direction as the original vector but a length of 1, and can be used for operations such as calculating directions, applying rotations, or implementing lighting models in graphics.
             * @tparam U An optional template parameter that specifies the return type of the normalized vector. This allows the function to return a vector with a different scalar type than the original vector components, which can be useful for precision or performance reasons. The default type is T, which is the scalar type of the original vector components.
             * @return A new vector that is the normalized version of this vector. The function calculates the length of this vector and divides each component by that length to produce a unit vector in the same direction. If the length of this vector is zero, the function returns a zero vector of the derived type to avoid division by zero.
             */
            template <typename U = T>
                requires std::floating_point<U>
            [[nodiscard]] Derived normalized() const noexcept
            {
                const U len = this->length();
                if (len == U{})
                    return Derived{};
                return (*static_cast<const Derived *>(this)) / static_cast<T>(len);
            }

            /**
             * @fn min
             * @brief Returns a vector that is the component-wise minimum of this vector and another vector. This function takes another vector of the same type and returns a new vector where each component is the smaller of the corresponding components from the two input vectors. The resulting vector represents the minimum bounds between the two vectors in the vector space, and can be used for operations such as calculating bounding boxes, clamping values, or implementing certain mathematical functions.
             * @param other The other vector to compare with this vector. The other vector must be of the same type (i.e., have the same scalar type and number of components) for this function to be valid.
             * @return A new vector that is the component-wise minimum of this vector and the other vector. Each component of the resulting vector is calculated as min(data()[i], other.data()[i]), where data()[i] is the corresponding component of this vector and other.data()[i] is the corresponding component of the other vector. The function creates a new instance of the derived vector type, performs the minimum calculation for each component, and returns it.
             */
            [[nodiscard]] constexpr Derived min(const Derived &other) const noexcept
            {
                Derived out;
                detail::min_to<T, N>(out.data(), data(), other.data());
                return out;
            }

            /**
             * @fn max
             * @brief Returns a vector that is the component-wise maximum of this vector and another vector. This function takes another vector of the same type and returns a new vector where each component is the larger of the corresponding components from the two input vectors. The resulting vector represents the maximum bounds between the two vectors in the vector space, and can be used for operations such as calculating bounding boxes, clamping values, or implementing certain mathematical functions.
             * @param other The other vector to compare with this vector. The other vector must be of the same type (i.e., have the same scalar type and number of components) for this function to be valid.
             * @return A new vector that is the component-wise maximum of this vector and the other vector. Each component of the resulting vector is calculated as max(data()[i], other.data()[i]), where data()[i] is the corresponding component of this vector and other.data()[i] is the corresponding component of the other vector. The function creates a new instance of the derived vector type, performs the maximum calculation for each component, and returns it.
             */
            [[nodiscard]] constexpr Derived max(const Derived &other) const noexcept
            {
                Derived out;
                detail::max_to<T, N>(out.data(), data(), other.data());
                return out;
            }

            /**
             * @fn clamp
             * @brief Returns a vector that is the component-wise clamping of this vector between two other vectors. This function takes two vectors of the same type, representing the lower and upper bounds, and returns a new vector where each component is clamped to be within the corresponding components of the lower and upper bound vectors. The resulting vector represents the clamped values of this vector within the specified bounds in the vector space, and can be used for operations such as constraining positions, limiting color values, or implementing certain mathematical functions.
             * @param lo The lower bound vector for clamping. Each component of this vector represents the minimum allowed value for the corresponding component of this vector. The lower bound vector must be of the same type (i.e., have the same scalar type and number of components) for this function to be valid.
             * @param hi The upper bound vector for clamping. Each component of this vector represents the maximum allowed value for the corresponding component of this vector. The upper bound vector must be of the same type (i.e., have the same scalar type and number of components) for this function to be valid.
             * @return A new vector that is the component-wise clamping of this vector between the lower and upper bound vectors. Each component of the resulting vector is calculated as clamp(data()[i], lo.data()[i], hi.data()[i]), where data()[i] is the corresponding component of this vector, lo.data()[i] is the corresponding component of the lower bound vector, and hi.data()[i] is the corresponding component of the upper bound vector. The function creates a new instance of the derived vector type, performs the clamping calculation for each component using a combination of min and max operations, and returns it.
             */
            [[nodiscard]] constexpr Derived clamp(const Derived &lo, const Derived &hi) const noexcept
            {
                return this->max(lo).min(hi);
            }

            /**
             * @fn swizzle
             * @brief Returns a new vector that is a swizzled version of this vector based on the specified component indices. This function takes a variadic list of component indices and returns a new vector where the components are rearranged according to those indices. The resulting vector represents a permutation of the original vector's components, which can be useful for operations such as reordering data, creating new vectors from existing ones, or implementing certain mathematical functions.
             * @tparam I A variadic list of non-type template parameters representing the component indices for swizzling. Each index must be less than N (the number of components in the original vector) for this function to be valid, and there must be at least one index provided.
             * @return A new vector that is the swizzled version of this vector based on the specified component indices. Each component of the resulting vector is taken from the original vector according to the corresponding index in the template parameter pack I. For example, if I is <2, 0>, then the resulting vector's first component will be data()[2] and its second component will be data()[0]. The function creates a new instance of a swizzle vector type (defined in detail::swizzle_vec_t) that has a number of components equal to the number of indices provided, populates it with the appropriate components from this vector, and returns it.
             */
            template <std::size_t... I>
                requires(sizeof...(I) > 0) && ((I < N) && ...)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, sizeof...(I)> swizzle() const noexcept
            {
                return detail::swizzle_vec_t<T, sizeof...(I)>{data()[I]...};
            }

            /**
             * @fn xx
             * @brief Provides convenient member functions for common swizzle patterns. These functions return new vectors that are specific rearrangements of the components of this vector, such as xx, xy, yx, etc. Each function corresponds to a specific swizzle pattern and returns a vector of the appropriate size based on the number of components in the original vector. The resulting vectors represent commonly used permutations of the original vector's components, which can be useful for operations such as duplicating components, reordering data, or implementing certain mathematical functions.
             * @tparam M An optional template parameter that specifies the minimum number of components required in the original vector for this swizzle shortcut to be valid. This allows the function to be conditionally compiled only for vectors that have enough components to support the specified swizzle pattern. For example, the xy() function requires at least 2 components (M >= 2), while the xyz() function requires at least 3 components (M >= 3). The default value for M is N, which means that by default, these functions will be available for any vector with enough components.
             * @return A new vector that is the result of applying the specific swizzle pattern corresponding to the member function. For example, the xx() function returns a vector where both components are taken from data()[0], while the xy() function returns a vector where the first component is data()[0] and the second component is data()[1]. Each function creates a new instance of a swizzle vector type (defined in detail::swizzle_vec_t) with the appropriate number of components based on the swizzle pattern, populates it with the corresponding components from this vector according to the pattern, and returns it.
             */
            template <std::size_t M = N>
                requires(M >= 2)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 2> xx() const noexcept
            {
                return this->template swizzle<0, 0>();
            }

            /**
             * @fn xy
             * @brief Returns a new vector that consists of the first two components of this vector. This member function is a convenient shortcut for swizzling the first two components of the vector, and is only available for vectors that have at least 2 components. The resulting vector represents the x and y components of the original vector, which can be useful for operations such as working with 2D positions, texture coordinates, or certain mathematical functions.
             * @tparam M An optional template parameter that specifies the minimum number of components required in the original vector for this function to be valid. This function requires at least 2 components (M >= 2) to be available.
             * @return A new vector that consists of the first two components of this vector. The first component of the resulting vector is data()[0] (the x component), and the second component is data()[1] (the y component). The function creates a new instance of a swizzle vector type (defined in detail::swizzle_vec_t) with 2 components, populates it with data()[0] and data()[1], and returns it.
             */
            template <std::size_t M = N>
                requires(M >= 2)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 2> xy() const noexcept
            {
                return this->template swizzle<0, 1>();
            }

            /**
             * @fn yx
             * @brief Returns a new vector that consists of the first two components of this vector in reverse order. This member function is a convenient shortcut for swizzling the first two components of the vector in reverse, and is only available for vectors that have at least 2 components. The resulting vector represents the y and x components of the original vector in reverse order, which can be useful for operations such as working with 2D positions, texture coordinates, or certain mathematical functions where the order of components matters.
             * @tparam M An optional template parameter that specifies the minimum number of components required in the original vector for this function to be valid. This function requires at least 2 components (M >= 2) to be available.
             * @return A new vector that consists of the first two components of this vector in reverse order. The first component of the resulting vector is data()[1] (the y component), and the second component is data()[0] (the x component). The function creates a new instance of a swizzle vector type (defined in detail::swizzle_vec_t) with 2 components, populates it with data()[1] and data()[0], and returns it.
             */
            template <std::size_t M = N>
                requires(M >= 2)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 2> yx() const noexcept
            {
                return this->template swizzle<1, 0>();
            }

            /**
             * @fn yy
             * @brief Returns a new vector that consists of the second component of this vector duplicated. This member function is a convenient shortcut for swizzling the second component of the vector, and is only available for vectors that have at least 2 components. The resulting vector represents the y component of the original vector duplicated in both components, which can be useful for operations such as working with 2D positions, texture coordinates, or certain mathematical functions where duplicating a component is needed.
             * @tparam M An optional template parameter that specifies the minimum number of components required in the original vector for this function to be valid. This function requires at least 2 components (M >= 2) to be available.
             * @return A new vector that consists of the second component of this vector duplicated. Both components of the resulting vector are data()[1] (the y component). The function creates a new instance of a swizzle vector type (defined in detail::swizzle_vec_t) with 2 components, populates it with data()[1] for both components, and returns it.
             */
            template <std::size_t M = N>
                requires(M >= 2)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 2> yy() const noexcept
            {
                return this->template swizzle<1, 1>();
            }

            /**
             * @fn xz
             * @brief Returns a new vector that consists of the first and third components of this vector. This member function is a convenient shortcut for swizzling the first and third components of the vector, and is only available for vectors that have at least 3 components. The resulting vector represents the x and z components of the original vector, which can be useful for operations such as working with 3D positions, texture coordinates, or certain mathematical functions.
             * @tparam M An optional template parameter that specifies the minimum number of components required in the original vector for this function to be valid. This function requires at least 3 components (M >= 3) to be available.
             * @return A new vector that consists of the first and third components of this vector. The first component of the resulting vector is data()[0] (the x component), and the second component is data()[2] (the z component). The function creates a new instance of a swizzle vector type (defined in detail::swizzle_vec_t) with 2 components, populates it with data()[0] and data()[2], and returns it.
             */
            template <std::size_t M = N>
                requires(M >= 3)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 2> xz() const noexcept
            {
                return this->template swizzle<0, 2>();
            }

            /**
             * @fn yz
             * @brief Returns a new vector that consists of the second and third components of this vector. This member function is a convenient shortcut for swizzling the second and third components of the vector, and is only available for vectors that have at least 3 components. The resulting vector represents the y and z components of the original vector, which can be useful for operations such as working with 3D positions, texture coordinates, or certain mathematical functions.
             * @tparam M An optional template parameter that specifies the minimum number of components required in the original vector for this function to be valid. This function requires at least 3 components (M >= 3) to be available.
             * @return A new vector that consists of the second and third components of this vector. The first component of the resulting vector is data()[1] (the y component), and the second component is data()[2] (the z component). The function creates a new instance of a swizzle vector type (defined in detail::swizzle_vec_t) with 2 components, populates it with data()[1] and data()[2], and returns it.
             */
            template <std::size_t M = N>
                requires(M >= 3)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 2> yz() const noexcept
            {
                return this->template swizzle<1, 2>();
            }

            /**
             * @fn yz
             * @brief Returns a new vector that consists of the third and first components of this vector. This member function is a convenient shortcut for swizzling the third and first components of the vector, and is only available for vectors that have at least 3 components. The resulting vector represents the z and x components of the original vector, which can be useful for operations such as working with 3D positions, texture coordinates, or certain mathematical functions.
             * @tparam M An optional template parameter that specifies the minimum number of components required in the original vector for this function to be valid. This function requires at least 3 components (M >= 3) to be available.
             * @return A new vector that consists of the third and first components of this vector. The first component of the resulting vector is data()[2] (the z component), and the second component is data()[0] (the x component). The function creates a new instance of a swizzle vector type (defined in detail::swizzle_vec_t) with 2 components, populates it with data()[2] and data()[0], and returns it.
             */
            template <std::size_t M = N>
                requires(M >= 3)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 2> zx() const noexcept
            {
                return this->template swizzle<2, 0>();
            }

            /**
             * @fn yz
             * @brief Returns a new vector that consists of the third and second components of this vector. This member function is a convenient shortcut for swizzling the third and second components of the vector, and is only available for vectors that have at least 3 components. The resulting vector represents the z and y components of the original vector, which can be useful for operations such as working with 3D positions, texture coordinates, or certain mathematical functions.
             * @tparam M An optional template parameter that specifies the minimum number of components required in the original vector for this function to be valid. This function requires at least 3 components (M >= 3) to be available.
             * @return A new vector that consists of the third and second components of this vector. The first component of the resulting vector is data()[2] (the z component), and the second component is data()[1] (the y component). The function creates a new instance of a swizzle vector type (defined in detail::swizzle_vec_t) with 2 components, populates it with data()[2] and data()[1], and returns it.
             */
            template <std::size_t M = N>
                requires(M >= 3)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 2> zy() const noexcept
            {
                return this->template swizzle<2, 1>();
            }

            /**
             * @fn xyz
             * @brief Returns a new vector that consists of the first three components of this vector. This member function is a convenient shortcut for swizzling the first three components of the vector, and is only available for vectors that have at least 3 components. The resulting vector represents the x, y, and z components of the original vector, which can be useful for operations such as working with 3D positions, texture coordinates, or certain mathematical functions.
             * @tparam M An optional template parameter that specifies the minimum number of components required in the original vector for this function to be valid. This function requires at least 3 components (M >= 3) to be available.
             * @return A new vector that consists of the first three components of this vector. The first component of the resulting vector is data()[0] (the x component), the second component is data()[1] (the y component), and the third component is data()[2] (the z component). The function creates a new instance of a swizzle vector type (defined in detail::swizzle_vec_t) with 3 components, populates it with data()[0], data()[1], and data()[2], and returns it.
             */
            template <std::size_t M = N>
                requires(M >= 3)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 3> xyz() const noexcept
            {
                return this->template swizzle<0, 1, 2>();
            }

            /**
             * @fn xzy
             * @brief Returns a new vector that consists of the first, third, and second components of this vector. This member function is a convenient shortcut for swizzling the first, third, and second components of the vector in that specific order, and is only available for vectors that have at least 3 components. The resulting vector represents the x, z, and y components of the original vector in that order, which can be useful for operations such as working with 3D positions, texture coordinates, or certain mathematical functions where a specific permutation of components is needed.
             * @tparam M An optional template parameter that specifies the minimum number of components required in the original vector for this function to be valid. This function requires at least 3 components (M >= 3) to be available.
             * @return A new vector that consists of the first, third, and second components of this vector. The first component of the resulting vector is data()[0] (the x component), the second component is data()[2] (the z component), and the third component is data()[1] (the y component). The function creates a new instance of a swizzle vector type (defined in detail::swizzle_vec_t) with 3 components, populates it with data()[0], data()[2], and data()[1], and returns it.
             */
            template <std::size_t M = N>
                requires(M >= 3)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 3> xzy() const noexcept
            {
                return this->template swizzle<0, 2, 1>();
            }

            /**
             * @fn yxz
             * @brief Returns a new vector that consists of the second, first, and third components of this vector. This member function is a convenient shortcut for swizzling the second, first, and third components of the vector in that specific order, and is only available for vectors that have at least 3 components. The resulting vector represents the y, x, and z components of the original vector in that order, which can be useful for operations such as working with 3D positions, texture coordinates, or certain mathematical functions where a specific permutation of components is needed.
             * @tparam M An optional template parameter that specifies the minimum number of components required in the original vector for this function to be valid. This function requires at least 3 components (M >= 3) to be available.
             * @return A new vector that consists of the second, first, and third components of this vector. The first component of the resulting vector is data()[1] (the y component), the second component is data()[0] (the x component), and the third component is data()[2] (the z component). The function creates a new instance of a swizzle vector type (defined in detail::swizzle_vec_t) with 3 components, populates it with data()[1], data()[0], and data()[2], and returns it.
             */
            template <std::size_t M = N>
                requires(M >= 3)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 3> yxz() const noexcept
            {
                return this->template swizzle<1, 0, 2>();
            }

            /**
             * @fn yzx
             * @brief Returns a new vector that consists of the second, third, and first components of this vector. This member function is a convenient shortcut for swizzling the second, third, and first components of the vector in that specific order, and is only available for vectors that have at least 3 components. The resulting vector represents the y, z, and x components of the original vector in that order, which can be useful for operations such as working with 3D positions, texture coordinates, or certain mathematical functions where a specific permutation of components is needed.
             * @tparam M An optional template parameter that specifies the minimum number of components required in the original vector for this function to be valid. This function requires at least 3 components (M >= 3) to be available.
             * @return A new vector that consists of the second, third, and first components of this vector. The first component of the resulting vector is data()[1] (the y component), the second component is data()[2] (the z component), and the third component is data()[0] (the x component). The function creates a new instance of a swizzle vector type (defined in detail::swizzle_vec_t) with 3 components, populates it with data()[1], data()[2], and data()[0], and returns it.
             */
            template <std::size_t M = N>
                requires(M >= 3)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 3> yzx() const noexcept
            {
                return this->template swizzle<1, 2, 0>();
            }

            /**
             * @fn zxy
             * @brief Returns a new vector that consists of the third, first, and second components of this vector. This member function is a convenient shortcut for swizzling the third, first, and second components of the vector in that specific order, and is only available for vectors that have at least 3 components. The resulting vector represents the z, x, and y components of the original vector in that order, which can be useful for operations such as working with 3D positions, texture coordinates, or certain mathematical functions where a specific permutation of components is needed.
             * @tparam M An optional template parameter that specifies the minimum number of components required in the original vector for this function to be valid. This function requires at least 3 components (M >= 3) to be available.
             * @return A new vector that consists of the third, first, and second components of this vector. The first component of the resulting vector is data()[2] (the z component), the second component is data()[0] (the x component), and the third component is data()[1] (the y component). The function creates a new instance of a swizzle vector type (defined in detail::swizzle_vec_t) with 3 components, populates it with data()[2], data()[0], and data()[1], and returns it.
             */
            template <std::size_t M = N>
                requires(M >= 3)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 3> zxy() const noexcept
            {
                return this->template swizzle<2, 0, 1>();
            }

            /**
             * @fn zxy
             * @brief Returns a new vector that consists of the third, second, and first components of this vector. This member function is a convenient shortcut for swizzling the third, second, and first components of the vector in that specific order, and is only available for vectors that have at least 3 components. The resulting vector represents the z, y, and x components of the original vector in that order, which can be useful for operations such as working with 3D positions, texture coordinates, or certain mathematical functions where a specific permutation of components is needed.
             * @tparam M An optional template parameter that specifies the minimum number of components required in the original vector for this function to be valid. This function requires at least 3 components (M >= 3) to be available.
             * @return A new vector that consists of the third, second, and first components of this vector. The first component of the resulting vector is data()[2] (the z component), the second component is data()[1] (the y component), and the third component is data()[0] (the x component). The function creates a new instance of a swizzle vector type (defined in detail::swizzle_vec_t) with 3 components, populates it with data()[2], data()[1], and data()[0], and returns it.
             */
            template <std::size_t M = N>
                requires(M >= 3)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 3> zyx() const noexcept
            {
                return this->template swizzle<2, 1, 0>();
            }

            /**
             * @fn zw
             * @brief Returns a new vector that consists of the third and fourth components of this vector. This member function is a convenient shortcut for swizzling the third and fourth components of the vector, and is only available for vectors that have at least 4 components. The resulting vector represents the z and w components of the original vector, which can be useful for operations such as working with 4D positions, homogeneous coordinates, or certain mathematical functions.
             * @tparam M An optional template parameter that specifies the minimum number of components required in the original vector for this function to be valid. This function requires at least 4 components (M >= 4) to be available.
             * @return A new vector that consists of the third and fourth components of this vector. The first component of the resulting vector is data()[2] (the z component), and the second component is data()[3] (the w component). The function creates a new instance of a swizzle vector type (defined in detail::swizzle_vec_t) with 2 components, populates it with data()[2] and data()[3], and returns it.
             */
            template <std::size_t M = N>
                requires(M >= 4)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 2> zw() const noexcept
            {
                return this->template swizzle<2, 3>();
            }

            /**
             * @fn wzy
             * @brief Returns a new vector that consists of the fourth and third components of this vector. This member function is a convenient shortcut for swizzling the fourth and third components of the vector in reverse order, and is only available for vectors that have at least 4 components. The resulting vector represents the w and z components of the original vector in reverse order, which can be useful for operations such as working with 4D positions, homogeneous coordinates, or certain mathematical functions where the order of components matters.
             * @tparam M An optional template parameter that specifies the minimum number of components required in the original vector for this function to be valid. This function requires at least 4 components (M >= 4) to be available.
             * @return A new vector that consists of the fourth and third components of this vector in reverse order. The first component of the resulting vector is data()[3] (the w component), and the second component is data()[2] (the z component). The function creates a new instance of a swizzle vector type (defined in detail::swizzle_vec_t) with 2 components, populates it with data()[3] and data()[2], and returns it.
             */
            template <std::size_t M = N>
                requires(M >= 4)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 4> xyzw() const noexcept
            {
                return this->template swizzle<0, 1, 2, 3>();
            }

            /**
             * @fn wzyx
             * @brief Returns a new vector that consists of the fourth, third, second, and first components of this vector in reverse order. This member function is a convenient shortcut for swizzling the fourth, third, second, and first components of the vector in reverse order, and is only available for vectors that have at least 4 components. The resulting vector represents the w, z, y, and x components of the original vector in reverse order, which can be useful for operations such as working with 4D positions, homogeneous coordinates, or certain mathematical functions where a specific permutation of components is needed.
             * @tparam M An optional template parameter that specifies the minimum number of components required in the original vector for this function to be valid. This function requires at least 4 components (M >= 4) to be available.
             * @return A new vector that consists of the fourth, third, second, and first components of this vector in reverse order. The first component of the resulting vector is data()[3] (the w component), the second component is data()[2] (the z component), the third component is data()[1] (the y component), and the fourth component is data()[0] (the x component). The function creates a new instance of a swizzle vector type (defined in detail::swizzle_vec_t) with 4 components, populates it with data()[3], data()[2], data()[1], and data()[0], and returns it.
             */
            template <std::size_t M = N>
                requires(M >= 4)
            [[nodiscard]] constexpr detail::swizzle_vec_t<T, 4> wzyx() const noexcept
            {
                return this->template swizzle<3, 2, 1, 0>();
            }
        };
    }

    /**
     * @struct vec
     * @brief A template struct representing a vector with a specified scalar type, number of components, and alignment. This struct provides a flexible and efficient way to represent vectors in mathematical computations, graphics programming, or any context where vector operations are needed. The vec struct is designed to be used with scalar types (such as float, double, int, etc.) and can have any number of components (N > 0). It also supports custom alignment for performance optimization on certain hardware architectures. The struct includes constructors for initializing the vector with a splat value or an initializer list, as well as member functions for accessing the underlying data and performing common vector operations.
     * @tparam T The scalar type of the vector components. This type must satisfy the is_vec_scalar_v trait, which means it should be a valid scalar type that can be used in vector operations (e.g., float, double, int).
     * @tparam N The number of components in the vector. This must be greater than 0 for the vec struct to be valid.
     * @tparam Align An optional template parameter specifying the alignment of the vector in memory. The default value is alignof(T), which means the vector will be aligned according to the requirements of its scalar type. Custom alignment can be specified for performance optimization on certain hardware architectures.
     */
    template <typename T, std::size_t N, std::size_t Align = alignof(T), typename = std::enable_if_t<is_vec_scalar_v<T>>>
    struct alignas(Align) vec : detail::vec_common<vec<T, N, Align, std::enable_if_t<is_vec_scalar_v<T>>>, T, N>
    {
        static_assert(N > 0, "vec<T,N>: N must be > 0");

        /**
         * @brief The underlying data storage for the vector components. This member variable is an array of type T with N elements, where T is the scalar type of the vector components and N is the number of components in the vector. The array is default-initialized to zero (or the default value for the scalar type) using empty braces {}. This means that when a vec instance is created, all components will be initialized to their default values unless explicitly set otherwise through constructors or member functions.
         */
        std::array<T, N> v{};

        /**
         * @fn constructors
         * @brief Constructors for initializing the vector. The vec struct provides multiple constructors to allow for flexible initialization of the vector components. The default constructor initializes all components to their default values (zero or equivalent). The constructor that takes a single T value allows for splatting that value across all components of the vector. The constructor that takes an initializer list allows for initializing the vector with a specific set of values, where any missing components will be filled with default values.
         */
        constexpr vec() noexcept = default;

        /**
         * @fn vec(T splat_value)
         * @brief Constructor that initializes all components of the vector to the same value (splatting). This constructor takes a single value of type T and assigns it to all components of the vector. This is useful for quickly creating a vector where all components are the same, such as a vector of ones or zeros. The constructor uses a helper function (detail::fill) to fill the underlying array with the specified splat value.
         * @param splat_value The value to be assigned to all components of the vector. This value will be replicated across all N components of the vector, resulting in a vector where each component has the same value.
         */
        explicit constexpr vec(T splat_value) noexcept
        {
            detail::fill<T, N>(data(), splat_value);
        }

        /**
         * @fn vec(std::initializer_list<T> init)
         * @brief Constructor that initializes the vector with a list of values. This constructor takes an initializer list of type T and uses it to initialize the components of the vector. The number of values provided in the initializer list can be less than or equal to N (the number of components in the vector). If fewer values are provided than the number of components, the remaining components will be initialized to their default values (zero or equivalent). This allows for flexible initialization of the vector with specific values while ensuring that all components are properly initialized.
         * @param init An initializer list containing the values to initialize the vector components. The constructor will copy values from this list into the underlying array, filling as many components as there are values in the list (up to N). Any remaining components that are not initialized from the list will be set to their default value.
         */
        constexpr vec(std::initializer_list<T> init) noexcept
        {
            const std::size_t count = (std::min)(init.size(), N);
            auto it = init.begin();
            for (std::size_t i = 0; i < count; ++i, ++it)
                data()[i] = *it;
            for (std::size_t i = count; i < N; ++i)
                data()[i] = T{};
        }

        /**
         * @fn data
         * @brief Member functions for accessing the underlying data of the vector. These functions provide access to the raw array of components that make up the vector. The non-const version returns a pointer to the array, allowing for modification of the components, while the const version returns a pointer to const, ensuring that the components cannot be modified through that pointer. These functions are useful for interoperability with APIs or functions that expect raw pointers to data, as well as for implementing certain operations that may require direct access to the underlying array.
         * @return A pointer to the underlying array of components in the vector. The non-const version allows for modification of the components, while the const version ensures that the components cannot be modified through that pointer.
         */
        [[nodiscard]] constexpr T *data() noexcept { return v.data(); }
        /**
         * @fn data (const version)
         * @brief Const version of the data member function for accessing the underlying data of the vector. This function returns a pointer to const, which means that the components of the vector cannot be modified through this pointer. This is useful for ensuring that the data is not accidentally modified when it is accessed in a read-only context, such as when passing the vector to a function that should not modify it or when performing operations that only require reading the components.
         * @return A pointer to the underlying array of components in the vector. This version of the function returns a pointer to const, indicating that the components cannot be modified through this pointer.
         */
        [[nodiscard]] constexpr const T *data() const noexcept { return v.data(); }
    };

    /**
     * @struct vec (specialization for 2 components)
     * @brief A specialization of the vec struct for vectors with 2 components. This specialization provides named member variables (x and y) for the components of the vector, which can improve code readability and make it more intuitive to access the components of a 2D vector. The specialization is designed to be used with scalar types (such as float, double, int, etc.) and supports custom alignment for performance optimization on certain hardware architectures. The struct includes constructors for initializing the vector with specific values or a splat value, as well as member functions for accessing the underlying data and performing common vector operations.
     * @tparam T The scalar type of the vector components. This type must satisfy the is_vec_scalar_v trait, which means it should be a valid scalar type that can be used in vector operations (e.g., float, double, int).
     * @tparam Align An optional template parameter specifying the alignment of the vector in memory. The default value is alignof(T), which means the vector will be aligned according to the requirements of its scalar type. Custom alignment can be specified for performance optimization on certain hardware architectures.
     */
    template <typename T, std::size_t Align>
    struct alignas(Align) vec<T,
                              2,
                              Align,
                              std::enable_if_t<is_vec_scalar_v<T>>> : detail::vec_common<vec<T,
                                                                                             2,
                                                                                             Align,
                                                                                             std::enable_if_t<is_vec_scalar_v<T>>>,
                                                                                         T, 2>
    {
        /**
         * @brief The underlying data storage for the vector components. In this specialization for 2 components, we use named member variables (x and y) instead of an array to represent the components of the vector. This allows for more intuitive access to the components and can improve code readability when working with 2D vectors. The member variables are default-initialized to zero (or the default value for the scalar type) using empty braces {}. This means that when a vec instance is created, both x and y will be initialized to their default values unless explicitly set otherwise through constructors or member functions.
         */
        using value_type = T;

        /**
         * @var x
         * @brief The first component of the vector, commonly referred to as the x component. This member variable represents the first dimension of the vector and is typically used to store the horizontal component in 2D space or the x coordinate in various mathematical contexts. It is default-initialized to zero (or the default value for the scalar type) using empty braces {}.
         */
        T x{};
        /**
         * @var y
         * @brief The second component of the vector, commonly referred to as the y component. This member variable represents the second dimension of the vector and is typically used to store the vertical component in 2D space or the y coordinate in various mathematical contexts. It is default-initialized to zero (or the default value for the scalar type) using empty braces {}.
         */
        T y{};

        /**
         * @fn vec
         * @brief Default constructor for the vec struct. This constructor initializes the x and y components of the vector to their default values (zero or equivalent) using empty braces {}. This means that when a vec instance is created using this constructor, both x and y will be initialized to their default values unless explicitly set otherwise through other constructors or member functions.
         */
        constexpr vec() noexcept = default;
        /**
         * @fn vec(T x_, T y_)
         * @brief Constructor that initializes the x and y components of the vector with specific values. This constructor takes two values of type T, where the first value initializes the x component and the second value initializes the y component. This allows for creating a 2D vector with specific coordinates or values for its components, which can be useful in various applications such as graphics programming, physics simulations, or mathematical computations.
         */
        constexpr vec(T x_, T y_) noexcept : x(x_), y(y_) {}

        /**
         * @fn vec(T splat_value)
         * @brief Constructor that initializes both the x and y components of the vector to the same value (splatting). This constructor takes a single value of type T and assigns it to both the x and y components of the vector. This is useful for quickly creating a 2D vector where both components are the same, such as a vector of ones or zeros. The constructor directly assigns the splat value to both x and y, resulting in a vector where each component has the same value.
         * @param splat_value The value to be assigned to both the x and y components of the vector. This value will be replicated across both components, resulting in a vector where x and y have the same value.
         */
        explicit constexpr vec(T splat_value) noexcept : x(splat_value), y(splat_value) {}

        /**
         * @fn vec(std::initializer_list<T> init)
         * @brief Constructor that initializes the vector with a list of values. This constructor takes an initializer list of type T and uses it to initialize the x and y components of the vector. The number of values provided in the initializer list can be less than or equal to 2 (the number of components in this specialization). If fewer values are provided than the number of components, the remaining components will be initialized to their default values (zero or equivalent). This allows for flexible initialization of the vector with specific values while ensuring that both components are properly initialized.
         * @param init An initializer list containing the values to initialize the x and y components of the vector. The constructor will copy values from this list into x and y, filling as many components as there are values in the list (up to 2). Any remaining components that are not initialized from the list will be set to their default value.
         */
        constexpr vec(std::initializer_list<T> init) noexcept
        {
            const std::size_t count = (std::min)(init.size(), std::size_t{2});
            auto it = init.begin();
            for (std::size_t i = 0; i < count; ++i, ++it)
                data()[i] = *it;
            for (std::size_t i = count; i < 2; ++i)
                data()[i] = T{};
        }

        /**
         * @fn data
         * @brief Member functions for accessing the underlying data of the vector. These functions provide access to the raw components of the vector (x and y) through a pointer. The non-const version returns a pointer to the first component (x), allowing for modification of both x and y through pointer arithmetic, while the const version returns a pointer to const, ensuring that the components cannot be modified through that pointer. These functions are useful for interoperability with APIs or functions that expect raw pointers to data, as well as for implementing certain operations that may require direct access to the underlying components.
         * @return A pointer to the first component (x) of the vector. The non-const version allows for modification of both x and y through pointer arithmetic, while the const version ensures that the components cannot be modified through that pointer.
         */
        [[nodiscard]] constexpr T *data() noexcept { return &x; }
        /**
         * @fn data (const version)
         * @brief Const version of the data member function for accessing the underlying data of the vector. This function returns a pointer to const, which means that the components of the vector cannot be modified through this pointer. This is useful for ensuring that the data is not accidentally modified when it is accessed in a read-only context, such as when passing the vector to a function that should not modify it or when performing operations that only require reading the components.
         * @return A pointer to the first component (x) of the vector. This version of the function returns a pointer to const, indicating that the components cannot be modified through that pointer.
         */
        [[nodiscard]] constexpr const T *data() const noexcept { return &x; }
    };

    /**
     * @struct vec (specialization for 3 components)
     * @brief Specialization of the vec struct for 3 components. This specialization provides a more convenient and efficient way to represent vectors with 3 components, which are common in graphics programming and mathematical computations involving 3D space. By defining this specialization, we can use named member variables (x, y, z) instead of an array for the components, which can improve code readability and allow for more intuitive access to the components. Additionally, this specialization includes a specific member function for calculating the cross product of two 3D vectors, which is a common operation in 3D graphics and physics simulations.
     * @tparam T The scalar type of the vector components. This type must satisfy the is_vec_scalar_v trait, which means it should be a valid scalar type that can be used in vector operations (e.g., float, double, int).
     * @tparam Align An optional template parameter specifying the alignment of the vector in memory. The default value is alignof(T), which means the vector will be aligned according to the requirements of its scalar type. Custom alignment can be specified for performance optimization on certain hardware architectures.
     */
    template <typename T, std::size_t Align>
    struct alignas(Align) vec<T,
                              3,
                              Align,
                              std::enable_if_t<is_vec_scalar_v<T>>> : detail::vec_common<vec<T,
                                                                                             3,
                                                                                             Align,
                                                                                             std::enable_if_t<is_vec_scalar_v<T>>>,
                                                                                         T, 3>
    {
        /**
         * @brief The underlying data storage for the vector components. In this specialization for 3 components, we use named member variables (x, y, and z) instead of an array to represent the components of the vector. This allows for more intuitive access to the components and can improve code readability when working with 3D vectors. The member variables are default-initialized to zero (or the default value for the scalar type) using empty braces {}. This means that when a vec instance is created, x, y, and z will be initialized to their default values unless explicitly set otherwise through constructors or member functions.
         */
        using value_type = T;

        /**
         * @var x
         * @brief The first component of the vector, commonly referred to as the x component. This member variable represents the first dimension of the vector and is typically used to store the horizontal component in 3D space or the x coordinate in various mathematical contexts. It is default-initialized to zero (or the default value for the scalar type) using empty braces {}.
         */
        T x{};
        /**
         * @var y
         * @brief The second component of the vector, commonly referred to as the y component. This member variable represents the second dimension of the vector and is typically used to store the vertical component in 3D space or the y coordinate in various mathematical contexts. It is default-initialized to zero (or the default value for the scalar type) using empty braces {}.
         */
        T y{};
        /**
         * @var z
         * @brief The third component of the vector, commonly referred to as the z component. This member variable represents the third dimension of the vector and is typically used to store the depth component in 3D space or the z coordinate in various mathematical contexts. It is default-initialized to zero (or the default value for the scalar type) using empty braces {}.
         */
        T z{};

        /**
         * @fn vec
         * @brief Default constructor for the vec struct. This constructor initializes the x, y, and z components of the vector to their default values (zero or equivalent) using empty braces {}. This means that when a vec instance is created using this constructor, x, y, and z will be initialized to their default values unless explicitly set otherwise through other constructors or member functions.
         */
        constexpr vec() noexcept = default;
        /**
         * @fn vec(T x_, T y_, T z_)
         * @brief Constructor that initializes the x, y, and z components of the vector with specific values. This constructor takes three values of type T, where the first value initializes the x component, the second value initializes the y component, and the third value initializes the z component. This allows for creating a 3D vector with specific coordinates or values for its components, which can be useful in various applications such as graphics programming, physics simulations, or mathematical computations involving 3D space.
         */
        constexpr vec(T x_, T y_, T z_) noexcept : x(x_), y(y_), z(z_) {}
        /**
         * @fn vec(T splat_value)
         * @brief Constructor that initializes all three components of the vector to the same value (splatting). This constructor takes a single value of type T and assigns it to the x, y, and z components of the vector. This is useful for quickly creating a 3D vector where all components are the same, such as a vector of ones or zeros. The constructor directly assigns the splat value to x, y, and z, resulting in a vector where each component has the same value.
         * @param splat_value The value to be assigned to all three components (x, y, and z) of the vector. This value will be replicated across all components, resulting in a vector where x, y, and z have the same value.
         */
        explicit constexpr vec(T splat_value) noexcept : x(splat_value), y(splat_value), z(splat_value) {}
        /**
         * @fn vec(std::initializer_list<T> init)
         * @brief Constructor that initializes the vector with a list of values. This constructor takes an initializer list of type T and uses it to initialize the x, y, and z components of the vector. The number of values provided in the initializer list can be less than or equal to 3 (the number of components in this specialization). If fewer values are provided than the number of components, the remaining components will be initialized to their default values (zero or equivalent). This allows for flexible initialization of the vector with specific values while ensuring that all components are properly initialized.
         * @param init An initializer list containing the values to initialize the x, y, and z components of the vector. The constructor will copy values from this list into x, y, and z, filling as many components as there are values in the list (up to 3). Any remaining components that are not initialized from the list will be set to their default value.
         */
        constexpr vec(std::initializer_list<T> init) noexcept
        {
            const std::size_t count = (std::min)(init.size(), std::size_t{3});
            auto it = init.begin();
            for (std::size_t i = 0; i < count; ++i, ++it)
                data()[i] = *it;
            for (std::size_t i = count; i < 3; ++i)
                data()[i] = T{};
        }

        /**
         * @fn data
         * @brief Member functions for accessing the underlying data of the vector. These functions provide access to the raw components of the vector (x, y, and z) through a pointer. The non-const version returns a pointer to the first component (x), allowing for modification of all components through pointer arithmetic, while the const version returns a pointer to const, ensuring that the components cannot be modified through that pointer. These functions are useful for interoperability with APIs or functions that expect raw pointers to data, as well as for implementing certain operations that may require direct access to the underlying components.
         * @return A pointer to the first component (x) of the vector. The non-const version allows for modification of all components through pointer arithmetic, while the const version ensures that the components cannot be modified through that pointer.
         */
        [[nodiscard]] constexpr T *data() noexcept { return &x; }
        /**
         * @fn data (const version)
         * @brief Const version of the data member function for accessing the underlying data of the vector. This function returns a pointer to const, which means that the components of the vector cannot be modified through this pointer. This is useful for ensuring that the data is not accidentally modified when it is accessed in a read-only context, such as when passing the vector to a function that should not modify it or when performing operations that only require reading the components.
         * @return A pointer to the first component (x) of the vector. This version of the function returns a pointer to const, indicating that the components cannot be modified through that pointer.
         */
        [[nodiscard]] constexpr const T *data() const noexcept { return &x; }

        /**
         * @fn cross
         * @brief Computes the cross product of this vector with another vector. The cross product is a mathematical operation that takes two 3D vectors and produces a third vector that is perpendicular to both input vectors. This member function is only available for vectors with exactly 3 components, as the cross product is defined specifically for 3D vectors. The resulting vector represents the cross product of this vector and the other vector, which can be useful in various applications such as calculating normals in graphics programming, determining the orientation of objects in 3D space, or performing certain mathematical computations.
         * @param other The other vector to compute the cross product with. This parameter must be of the same type and have the same number of components (3) as this vector for the function to be valid.
         * @return A new vector that represents the cross product of this vector and the other vector. The components of the resulting vector are calculated using the formula: (y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x). The function creates a new instance of vec with 3 components, populates it with the calculated values, and returns it.
         */
        [[nodiscard]] constexpr vec cross(const vec &other) const noexcept
        {
            return vec{
                y * other.z - z * other.y,
                z * other.x - x * other.z,
                x * other.y - y * other.x,
            };
        }
    };

    /**
     * @struct vec (specialization for 4 components)
     * @brief Specialization of the vec struct for 4 components. This specialization provides a more convenient and efficient way to represent vectors with 4 components, which are common in graphics programming and mathematical computations involving 4D space or homogeneous coordinates. By defining this specialization, we can use named member variables (x, y, z, w) instead of an array for the components, which can improve code readability and allow for more intuitive access to the components. Additionally, this specialization includes specific member functions for swizzling the components of the vector, which can be useful for operations such as working with 4D positions, homogeneous coordinates, or certain mathematical functions where specific permutations of components are needed.
     * @tparam T The scalar type of the vector components. This type must satisfy the is_vec_scalar_v trait, which means it should be a valid scalar type that can be used in vector operations (e.g., float, double, int).
     * @tparam Align An optional template parameter specifying the alignment of the vector in memory. The default value is alignof(T), which means the vector will be aligned according to the requirements of its scalar type. Custom alignment can be specified for performance optimization on certain hardware architectures.
     */
    template <typename T, std::size_t Align>
    struct alignas(Align) vec<T,
                              4,
                              Align,
                              std::enable_if_t<is_vec_scalar_v<T>>> : detail::vec_common<vec<T,
                                                                                             4,
                                                                                             Align,
                                                                                             std::enable_if_t<is_vec_scalar_v<T>>>,
                                                                                         T, 4>
    {
        /**
         * @brief The underlying data storage for the vector components. In this specialization for 4 components, we use named member variables (x, y, z, and w) instead of an array to represent the components of the vector. This allows for more intuitive access to the components and can improve code readability when working with 4D vectors. The member variables are default-initialized to zero (or the default value for the scalar type) using empty braces {}. This means that when a vec instance is created, x, y, z, and w will be initialized to their default values unless explicitly set otherwise through constructors or member functions.
         */
        using value_type = T;

        /**
         * @var x
         * @brief The first component of the vector, commonly referred to as the x component.
         */
        T x{};
        /**
         * @var y
         * @brief The second component of the vector, commonly referred to as the y component.
         */
        T y{};
        /**
         * @var z
         * @brief The third component of the vector, commonly referred to as the z component.
         */
        T z{};
        /**
         * @var w
         * @brief The fourth component of the vector, commonly referred to as the w component. In graphics programming, the w component is often used in homogeneous coordinates to represent the perspective division factor or to distinguish between points and vectors in 3D space.
         */
        T w{};

        /**
         * @fn vec
         * @brief Default constructor for the vec struct. This constructor initializes the x, y, z, and w components of the vector to their default values (zero or equivalent) using empty braces {}. This means that when a vec instance is created using this constructor, x, y, z, and w will be initialized to their default values unless explicitly set otherwise through other constructors or member functions.
         */
        constexpr vec() noexcept = default;
        /**
         * @fn vec(T x_, T y_, T z_, T w_)
         * @brief Constructor that initializes the x, y, z, and w components of the vector with specific values. This constructor takes four values of type T, where the first value initializes the x component, the second value initializes the y component, the third value initializes the z component, and the fourth value initializes the w component. This allows for creating a 4D vector with specific coordinates or values for its components, which can be useful in various applications such as graphics programming, physics simulations, or mathematical computations involving 4D space or homogeneous coordinates.
         */
        constexpr vec(T x_, T y_, T z_, T w_) noexcept : x(x_), y(y_), z(z_), w(w_) {}
        /**
         * @fn vec(T splat_value)
         * @brief Constructor that initializes all four components of the vector to the same value (splatting). This constructor takes a single value of type T and assigns it to the x, y, z, and w components of the vector. This is useful for quickly creating a 4D vector where all components are the same, such as a vector of ones or zeros. The constructor directly assigns the splat value to x, y, z, and w, resulting in a vector where each component has the same value.
         * @param splat_value The value to be assigned to all four components (x, y, z, and w) of the vector. This value will be replicated across all components, resulting in a vector where x, y, z, and w have the same value.
         */
        explicit constexpr vec(T splat_value) noexcept : x(splat_value), y(splat_value), z(splat_value), w(splat_value) {}
        /**
         * @fn vec(std::initializer_list<T> init)
         * @brief Constructor that initializes the vector with a list of values. This constructor takes an initializer list of type T and uses it to initialize the x, y, z, and w components of the vector. The number of values provided in the initializer list can be less than or equal to 4 (the number of components in this specialization). If fewer values are provided than the number of components, the remaining components will be initialized to their default values (zero or equivalent). This allows for flexible initialization of the vector with specific values while ensuring that all components are properly initialized.
         * @param init An initializer list containing the values to initialize the x, y, z, and w components of the vector. The constructor will copy values from this list into x, y, z, and w, filling as many components as there are values in the list (up to 4). Any remaining components that are not initialized from the list will be set to their default value.
         */
        constexpr vec(std::initializer_list<T> init) noexcept
        {
            const std::size_t count = (std::min)(init.size(), std::size_t{4});
            auto it = init.begin();
            for (std::size_t i = 0; i < count; ++i, ++it)
                data()[i] = *it;
            for (std::size_t i = count; i < 4; ++i)
                data()[i] = T{};
        }

        /**
         * @fn data
         * @brief Member functions for accessing the underlying data of the vector. These functions provide access to the raw components of the vector (x, y, z, and w) through a pointer. The non-const version returns a pointer to the first component (x), allowing for modification of all components through pointer arithmetic, while the const version returns a pointer to const, ensuring that the components cannot be modified through that pointer. These functions are useful for interoperability with APIs or functions that expect raw pointers to data, as well as for implementing certain operations that may require direct access to the underlying components.
         * @return A pointer to the first component (x) of the vector. The non-const version allows for modification of all components through pointer arithmetic, while the const version ensures that the components cannot be modified through that pointer.
         */
        [[nodiscard]] constexpr T *data() noexcept { return &x; }
        /**
         * @fn data (const version)
         * @brief Const version of the data member function for accessing the underlying data of the vector. This function returns a pointer to const, which means that the components of the vector cannot be modified through this pointer. This is useful for ensuring that the data is not accidentally modified when it is accessed in a read-only context, such as when passing the vector to a function that should not modify it or when performing operations that only require reading the components.
         * @return A pointer to the first component (x) of the vector. This version of the function returns a pointer to const, indicating that the components cannot be modified through that pointer.
         */
        [[nodiscard]] constexpr const T *data() const noexcept { return &x; }
    };

    /**
     * @typedef vec2
     * @brief Type alias for a 2D vector with a specified scalar type. This type alias provides a convenient name for a vector with 2 components (x and y) of a specific scalar type T. By using this type alias, we can easily refer to a 2D vector without having to specify the template parameters each time. For example, vec2f represents a 2D vector with float components, while vec2d represents a 2D vector with double components. This type alias improves code readability and makes it easier to work with 2D vectors in various applications such as graphics programming, physics simulations, or mathematical computations involving 2D space.
     * @tparam T The scalar type of the vector components. This type must satisfy the is_vec_scalar_v trait, which means it should be a valid scalar type that can be used in vector operations (e.g., float, double, int).
     */
    template <typename T>
    using vec2 = vec<T, 2>;
    /**
     * @typedef vec3
     * @brief Type alias for a 3D vector with a specified scalar type. This type alias provides a convenient name for a vector with 3 components (x, y, and z) of a specific scalar type T. By using this type alias, we can easily refer to a 3D vector without having to specify the template parameters each time. For example, vec3f represents a 3D vector with float components, while vec3d represents a 3D vector with double components. This type alias improves code readability and makes it easier to work with 3D vectors in various applications such as graphics programming, physics simulations, or mathematical computations involving 3D space.
     * @tparam T The scalar type of the vector components. This type must satisfy the is_vec_scalar_v trait, which means it should be a valid scalar type that can be used in vector operations (e.g., float, double, int).
     */
    template <typename T>
    using vec3 = vec<T, 3>;
    /**
     * @typedef vec4
     * @brief Type alias for a 4D vector with a specified scalar type. This type alias provides a convenient name for a vector with 4 components (x, y, z, and w) of a specific scalar type T. By using this type alias, we can easily refer to a 4D vector without having to specify the template parameters each time. For example, vec4f represents a 4D vector with float components, while vec4d represents a 4D vector with double components. This type alias improves code readability and makes it easier to work with 4D vectors in various applications such as graphics programming, physics simulations, or mathematical computations involving 4D space or homogeneous coordinates.
     * @tparam T The scalar type of the vector components. This type must satisfy the is_vec_scalar_v trait, which means it should be a valid scalar type that can be used in vector operations (e.g., float, double, int).
     */
    template <typename T>
    using vec4 = vec<T, 4>;

    /**
     * @typedef vec2f
     * @brief Type alias for a 2D vector with float components. This type alias provides a convenient name for a vector with 2 components (x and y) of type float. By using this type alias, we can easily refer to a 2D vector with float components without having to specify the template parameters each time. For example, vec2f represents a 2D vector with float components, which is commonly used in graphics programming and mathematical computations involving 2D space. This type alias improves code readability and makes it easier to work with 2D vectors in various applications.
     */
    using vec2f = vec2<float>;
    /**
     * @typedef vec3f
     * @brief Type alias for a 3D vector with float components. This type alias provides a convenient name for a vector with 3 components (x, y, and z) of type float. By using this type alias, we can easily refer to a 3D vector with float components without having to specify the template parameters each time. For example, vec3f represents a 3D vector with float components, which is commonly used in graphics programming and mathematical computations involving 3D space. This type alias improves code readability and makes it easier to work with 3D vectors in various applications.
     */
    using vec3f = vec3<float>;
    /**
     * @typedef vec4f
     * @brief Type alias for a 4D vector with float components. This type alias provides a convenient name for a vector with 4 components (x, y, z, and w) of type float. By using this type alias, we can easily refer to a 4D vector with float components without having to specify the template parameters each time. For example, vec4f represents a 4D vector with float components, which is commonly used in graphics programming and mathematical computations involving 4D space or homogeneous coordinates. This type alias improves code readability and makes it easier to work with 4D vectors in various applications.
     */
    using vec4f = vec4<float>;

    /**
     * @typedef vec2d
     * @brief Type alias for a 2D vector with double components. This type alias provides a convenient name for a vector with 2 components (x and y) of type double. By using this type alias, we can easily refer to a 2D vector with double components without having to specify the template parameters each time. For example, vec2d represents a 2D vector with double components, which is commonly used in graphics programming and mathematical computations involving 2D space where higher precision is required. This type alias improves code readability and makes it easier to work with 2D vectors in various applications.
     */
    using vec2d = vec2<double>;
    /**
     * @typedef vec3d
     * @brief Type alias for a 3D vector with double components. This type alias provides a convenient name for a vector with 3 components (x, y, and z) of type double. By using this type alias, we can easily refer to a 3D vector with double components without having to specify the template parameters each time. For example, vec3d represents a 3D vector with double components, which is commonly used in graphics programming and mathematical computations involving 3D space where higher precision is required. This type alias improves code readability and makes it easier to work with 3D vectors in various applications.
     */
    using vec3d = vec3<double>;
    /**
     * @typedef vec4d
     * @brief Type alias for a 4D vector with double components. This type alias provides a convenient name for a vector with 4 components (x, y, z, and w) of type double. By using this type alias, we can easily refer to a 4D vector with double components without having to specify the template parameters each time. For example, vec4d represents a 4D vector with double components, which is commonly used in graphics programming and mathematical computations involving 4D space or homogeneous coordinates where higher precision is required. This type alias improves code readability and makes it easier to work with 4D vectors in various applications.
     */
    using vec4d = vec4<double>;

} // namespace catalyst::math
