/**
 * @file simd_array_ops.hpp
 * @brief Internal SIMD-optimized array operations for vectors and matrices.
 * @details This header defines internal utility functions for performing SIMD-optimized operations on arrays of fundamental types (e.g. float, double, int32_t, etc.) that are used in the implementation of vector and matrix types in the Catalyst Math library. The functions include loading and storing data to/from SIMD registers, as well as performing element-wise addition, subtraction, multiplication, and division on arrays of values. These functions are designed to be efficient and take advantage of SIMD capabilities when available, while also providing fallback implementations for cases where SIMD is not supported or when the array size is not compatible with the SIMD width. The functions are defined in the catalyst::math::detail namespace, as they are intended for internal use within the library and are not part of the public API.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include <catalyst/math/simd.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>

/**
 * @namespace catalyst::math::detail
 * @brief The catalyst::math::detail namespace contains internal implementation details for the Catalyst Math library. This includes utility functions, type traits, and other components that are not intended to be part of the public API but are used internally by the library to implement the functionality of vectors, matrices, quaternions, and other mathematical types. By placing these components in a nested detail namespace, we can clearly indicate that they are for internal use only and help prevent accidental usage by users of the library. The contents of this namespace may change without notice, as they are not subject to the same stability guarantees as the public API.
 */
namespace catalyst::math::detail
{
    /**
     * @struct simd_pack
     * @tparam T The fundamental type for which to define the SIMD pack (e.g. float, double, int32_t, etc.).
     * @brief A type trait that defines the SIMD pack type and operations for a given fundamental type T. The simd_pack struct provides information about whether a SIMD pack is available for the type T, as well as the corresponding SIMD type, the number of lanes in the SIMD pack, and functions for loading and storing data to/from SIMD registers. Specializations of simd_pack are provided for supported types, while the primary template indicates that no SIMD pack is available for unsupported types.
     */
    template <typename T>
    struct simd_pack
    {
        /**
         * @brief Indicates whether a SIMD pack is available for the type T. The primary template defaults to false, and specializations for supported types will set this to true.
         */
        static constexpr bool available = false;
    };

    /**
     * @struct simd_pack<float>
     * @brief Specialization of simd_pack for float type, defining the SIMD pack type and operations for float values.
     * @details This specialization indicates that a SIMD pack is available for float type, using the f32x4 SIMD type which contains 4 lanes of float values. The load and store functions are defined to load and store data from/to memory using the appropriate SIMD instructions for float values.
     */
    template <>
    struct simd_pack<float>
    {
        /**
         * @brief The SIMD pack type for float values, which is f32x4 containing 4 lanes of float values.
         */
        using type = f32x4;
        /**
         * @brief Indicates that a SIMD pack is available for float type.
         */
        static constexpr bool available = true;
        /**
         * @brief The number of lanes in the SIMD pack for float type, which is 4 for f32x4.
         */
        static constexpr std::size_t lanes = 4;

        /**
         * @fn load
         * @brief Loads a SIMD pack of float values from memory. The input pointer should point to an array of float values, and the function will load the appropriate number of values into a SIMD register using unaligned load instructions.
         * @param p Pointer to an array of float values to load into the SIMD pack. The pointer does not need to be aligned.
         * @return A SIMD pack containing the loaded float values.
         * @note The load function uses unaligned load instructions, which means that the input pointer does not need to be aligned to the SIMD width. This allows for more flexible usage of the function, as it can handle cases where the data is not stored in a SIMD-friendly manner. However, it may be less efficient than aligned loads if the data is not properly aligned in memory.
         */
        static type load(const float *p) noexcept { return type::load_unaligned(p); }
        /**
         * @fn store
         * @brief Stores a SIMD pack of float values to memory. The input pointer should point to an array of float values where the SIMD pack will be stored, and the function will store the values from the SIMD register to memory using unaligned store instructions.
         * @param p Pointer to an array of float values where the SIMD pack will be stored. The pointer does not need to be aligned.
         * @param v The SIMD pack containing the float values to store to memory.
         * @note The store function uses unaligned store instructions, which means that the input pointer does not need to be aligned to the SIMD width. This allows for more flexible usage of the function, as it can handle cases where the data is not stored in a SIMD-friendly manner. However, it may be less efficient than aligned stores if the data is not properly aligned in memory.
         */
        static void store(float *p, type v) noexcept { v.store_unaligned(p); }
    };

    /**
     * @struct simd_pack<double>
     * @brief Specialization of simd_pack for double type, defining the SIMD pack type and operations for double values.
     * @details This specialization indicates that a SIMD pack is available for double type, using the f64x2 SIMD type which contains 2 lanes of double values. The load and store functions are defined to load and store data from/to memory using the appropriate SIMD instructions for double values.
     */
    template <>
    struct simd_pack<double>
    {
        /**
         * @brief The SIMD pack type for double values, which is f64x2 containing 2 lanes of double values.
         */
        using type = f64x2;
        /**
         * @brief Indicates that a SIMD pack is available for double type.
         */
        static constexpr bool available = true;
        /**
         * @brief The number of lanes in the SIMD pack for double type, which is 2 for f64x2.
         */
        static constexpr std::size_t lanes = 2;

        /**
         * @fn load
         * @brief Loads a SIMD pack of double values from memory. The input pointer should point to an array of double values, and the function will load the appropriate number of values into a SIMD register using unaligned load instructions.
         * @param p Pointer to an array of double values to load into the SIMD pack. The pointer does not need to be aligned.
         * @return A SIMD pack containing the loaded double values.
         * @note The load function uses unaligned load instructions, which means that the input pointer does not need to be aligned to the SIMD width. This allows for more flexible usage of the function, as it can handle cases where the data is not stored in a SIMD-friendly manner. However, it may be less efficient than aligned loads if the data is not properly aligned in memory.
         */
        static type load(const double *p) noexcept { return type::load_unaligned(p); }
        /**
         * @fn store
         * @brief Stores a SIMD pack of double values to memory. The input pointer should point to an array of double values where the SIMD pack will be stored, and the function will store the values from the SIMD register to memory using unaligned store instructions.
         * @param p Pointer to an array of double values where the SIMD pack will be stored. The pointer does not need to be aligned.
         * @param v The SIMD pack containing the double values to store to memory.
         * @note The store function uses unaligned store instructions, which means that the input pointer does not need to be aligned to the SIMD width. This allows for more flexible usage of the function, as it can handle cases where the data is not stored in a SIMD-friendly manner. However, it may be less efficient than aligned stores if the data is not properly aligned in memory.
         */
        static void store(double *p, type v) noexcept { v.store_unaligned(p); }
    };

    /**
     * @struct simd_pack<std::int32_t>
     * @brief Specialization of simd_pack for std::int32_t type, defining the SIMD pack type and operations for 32-bit signed integer values.
     * @details This specialization indicates that a SIMD pack is available for std::int32_t type, using the i32x4 SIMD type which contains 4 lanes of int32_t values. The load and store functions are defined to load and store data from/to memory using the appropriate SIMD instructions for int32_t values.
     */
    template <>
    struct simd_pack<std::int32_t>
    {
        /**
         * @brief The SIMD pack type for std::int32_t values, which is i32x4 containing 4 lanes of int32_t values.
         */
        using type = i32x4;
        /**
         * @brief Indicates that a SIMD pack is available for std::int32_t type.
         */
        static constexpr bool available = true;
        /**
         * @brief The number of lanes in the SIMD pack for std::int32_t type, which is 4 for i32x4.
         */
        static constexpr std::size_t lanes = 4;

        /**
         * @fn load
         * @brief Loads a SIMD pack of std::int32_t values from memory. The input pointer should point to an array of std::int32_t values, and the function will load the appropriate number of values into a SIMD register using unaligned load instructions.
         * @param p Pointer to an array of std::int32_t values to load into the SIMD pack. The pointer does not need to be aligned.
         * @return A SIMD pack containing the loaded std::int32_t values.
         * @note The load function uses unaligned load instructions, which means that the input pointer does not need to be aligned to the SIMD width. This allows for more flexible usage of the function, as it can handle cases where the data is not stored in a SIMD-friendly manner. However, it may be less efficient than aligned loads if the data is not properly aligned in memory.
         */
        static type load(const std::int32_t *p) noexcept { return type::load_unaligned(p); }
        /**
         * @fn store
         * @brief Stores a SIMD pack of std::int32_t values to memory. The input pointer should point to an array of std::int32_t values where the SIMD pack will be stored, and the function will store the values from the SIMD register to memory using unaligned store instructions.
         * @param p Pointer to an array of std::int32_t values where the SIMD pack will be stored. The pointer does not need to be aligned.
         * @param v The SIMD pack containing the std::int32_t values to store to memory.
         * @note The store function uses unaligned store instructions, which means that the input pointer does not need to be aligned to the SIMD width. This allows for more flexible usage of the function, as it can handle cases where the data is not stored in a SIMD-friendly manner. However, it may be less efficient than aligned stores if the data is not properly aligned in memory.
         */
        static void store(std::int32_t *p, type v) noexcept { v.store_unaligned(p); }
    };

    /**
     * @struct simd_pack<std::uint32_t>
     * @brief Specialization of simd_pack for std::uint32_t type, defining the SIMD pack type and operations for 32-bit unsigned integer values.
     * @details This specialization indicates that a SIMD pack is available for std::uint32_t type, using the u32x4 SIMD type which contains 4 lanes of uint32_t values. The load and store functions are defined to load and store data from/to memory using the appropriate SIMD instructions for uint32_t values.
     */
    template <>
    struct simd_pack<std::uint32_t>
    {
        /**
         * @brief The SIMD pack type for std::uint32_t values, which is u32x4 containing 4 lanes of uint32_t values.
         */
        using type = u32x4;
        /**
         * @brief Indicates that a SIMD pack is available for std::uint32_t type.
         */
        static constexpr bool available = true;
        /**
         * @brief The number of lanes in the SIMD pack for std::uint32_t type, which is 4 for u32x4.
         */
        static constexpr std::size_t lanes = 4;

        /**
         * @fn load
         * @brief Loads a SIMD pack of std::uint32_t values from memory. The input pointer should point to an array of std::uint32_t values, and the function will load the appropriate number of values into a SIMD register using unaligned load instructions.
         * @param p Pointer to an array of std::uint32_t values to load into the SIMD pack. The pointer does not need to be aligned.
         * @return A SIMD pack containing the loaded std::uint32_t values.
         * @note The load function uses unaligned load instructions, which means that the input pointer does not need to be aligned to the SIMD width. This allows for more flexible usage of the function, as it can handle cases where the data is not stored in a SIMD-friendly manner. However, it may be less efficient than aligned loads if the data is not properly aligned in memory.
         */
        static type load(const std::uint32_t *p) noexcept { return type::load_unaligned(p); }
        /**
         * @fn store
         * @brief Stores a SIMD pack of std::uint32_t values to memory. The input pointer should point to an array of std::uint32_t values where the SIMD pack will be stored, and the function will store the values from the SIMD register to memory using unaligned store instructions.
         * @param p Pointer to an array of std::uint32_t values where the SIMD pack will be stored. The pointer does not need to be aligned.
         * @param v The SIMD pack containing the std::uint32_t values to store to memory.
         * @note The store function uses unaligned store instructions, which means that the input pointer does not need to be aligned to the SIMD width. This allows for more flexible usage of the function, as it can handle cases where the data is not stored in a SIMD-friendly manner. However, it may be less efficient than aligned stores if the data is not properly aligned in memory.
         */
        static void store(std::uint32_t *p, type v) noexcept { v.store_unaligned(p); }
    };

    /**
     * @struct simd_pack<std::int64_t>
     * @brief Specialization of simd_pack for std::int64_t type, defining the SIMD pack type and operations for 64-bit signed integer values.
     * @details This specialization indicates that a SIMD pack is available for std::int64_t type, using the i64x2 SIMD type which contains 2 lanes of int64_t values. The load and store functions are defined to load and store data from/to memory using the appropriate SIMD instructions for int64_t values.
     */
    template <>
    struct simd_pack<std::int64_t>
    {
        /**
         * @brief The SIMD pack type for std::int64_t values, which is i64x2 containing 2 lanes of int64_t values.
         */
        using type = i64x2;
        /**
         * @brief Indicates that a SIMD pack is available for std::int64_t type.
         */
        static constexpr bool available = true;
        /**
         * @brief The number of lanes in the SIMD pack for std::int64_t type, which is 2 for i64x2.
         */
        static constexpr std::size_t lanes = 2;

        /**
         * @fn load
         * @brief Loads a SIMD pack of std::int64_t values from memory. The input pointer should point to an array of std::int64_t values, and the function will load the appropriate number of values into a SIMD register using unaligned load instructions.
         * @param p Pointer to an array of std::int64_t values to load into the SIMD pack. The pointer does not need to be aligned.
         * @return A SIMD pack containing the loaded std::int64_t values.
         * @note The load function uses unaligned load instructions, which means that the input pointer does not need to be aligned to the SIMD width. This allows for more flexible usage of the function, as it can handle cases where the data is not stored in a SIMD-friendly manner. However, it may be less efficient than aligned loads if the data is not properly aligned in memory.
         */
        static type load(const std::int64_t *p) noexcept { return type::load_unaligned(p); }
        /**
         * @fn store
         * @brief Stores a SIMD pack of std::int64_t values to memory. The input pointer should point to an array of std::int64_t values where the SIMD pack will be stored, and the function will store the values from the SIMD register to memory using unaligned store instructions.
         * @param p Pointer to an array of std::int64_t values where the SIMD pack will be stored. The pointer does not need to be aligned.
         * @param v The SIMD pack containing the std::int64_t values to store to memory.
         * @note The store function uses unaligned store instructions, which means that the input pointer does not need to be aligned to the SIMD width. This allows for more flexible usage of the function, as it can handle cases where the data is not stored in a SIMD-friendly manner. However, it may be less efficient than aligned stores if the data is not properly aligned in memory.
         */
        static void store(std::int64_t *p, type v) noexcept { v.store_unaligned(p); }
    };

    /**
     * @struct simd_pack<std::uint64_t>
     * @brief Specialization of simd_pack for std::uint64_t type, defining the SIMD pack type and operations for 64-bit unsigned integer values.
     * @details This specialization indicates that a SIMD pack is available for std::uint64_t type, using the u64x2 SIMD type which contains 2 lanes of uint64_t values. The load and store functions are defined to load and store data from/to memory using the appropriate SIMD instructions for uint64_t values.
     */
    template <>
    struct simd_pack<std::uint64_t>
    {
        /**
         * @brief The SIMD pack type for std::uint64_t values, which is u64x2 containing 2 lanes of uint64_t values.
         */
        using type = u64x2;
        /**
         * @brief Indicates that a SIMD pack is available for std::uint64_t type.
         */
        static constexpr bool available = true;
        /**
         * @brief The number of lanes in the SIMD pack for std::uint64_t type, which is 2 for u64x2.
         */
        static constexpr std::size_t lanes = 2;

        /**
         * @fn load
         * @brief Loads a SIMD pack of std::uint64_t values from memory. The input pointer should point to an array of std::uint64_t values, and the function will load the appropriate number of values into a SIMD register using unaligned load instructions.
         * @param p Pointer to an array of std::uint64_t values to load into the SIMD pack. The pointer does not need to be aligned.
         * @return A SIMD pack containing the loaded std::uint64_t values.
         * @note The load function uses unaligned load instructions, which means that the input pointer does not need to be aligned to the SIMD width. This allows for more flexible usage of the function, as it can handle cases where the data is not stored in a SIMD-friendly manner. However, it may be less efficient than aligned loads if the data is not properly aligned in memory.
         */
        static type load(const std::uint64_t *p) noexcept { return type::load_unaligned(p); }
        /**
         * @fn store
         * @brief Stores a SIMD pack of std::uint64_t values to memory. The input pointer should point to an array of std::uint64_t values where the SIMD pack will be stored, and the function will store the values from the SIMD register to memory using unaligned store instructions.
         * @param p Pointer to an array of std::uint64_t values where the SIMD pack will be stored. The pointer does not need to be aligned.
         * @param v The SIMD pack containing the std::uint64_t values to store to memory.
         * @note The store function uses unaligned store instructions, which means that the input pointer does not need to be aligned to the SIMD width. This allows for more flexible usage of the function, as it can handle cases where the data is not stored in a SIMD-friendly manner. However, it may be less efficient than aligned stores if the data is not properly aligned in memory.
         */
        static void store(std::uint64_t *p, type v) noexcept { v.store_unaligned(p); }
    };

    /**
     * @brief A helper variable template to check if a SIMD pack is available for a given type T. This variable will be true if simd_pack<T>::available is true, indicating that a SIMD pack is defined for the type T, and false otherwise. This can be used in compile-time checks to enable or disable SIMD optimizations based on the availability of a SIMD pack for the type being used.
     * @tparam T The type for which to check SIMD pack availability.
     */
    template <typename T>
    inline constexpr bool has_simd_pack_v = simd_pack<T>::available;

    /**
     * @brief A helper variable template to check if a type T with a given size N is compatible with SIMD operations. This variable will be true if there is a SIMD pack available for the type T and if the size N is a multiple of the number of lanes in the SIMD pack for type T. This can be used to determine if SIMD optimizations can be applied to operations on arrays of type T with size N.
     * @tparam T The type of the elements in the array.
     * @tparam N The size of the array.
     */
    template <typename T, std::size_t N>
    inline constexpr bool simd_compatible_v = has_simd_pack_v<T> && (N % simd_pack<T>::lanes == 0);

    /**
     * @fn fill
     * @brief Fills an array of type T and size N with a given value. This function will set each element of the array to the specified value. It is marked as constexpr, allowing it to be used in compile-time contexts, and it is noexcept, indicating that it does not throw exceptions.
     * @param dst Pointer to the array of type T and size N that will be filled with the specified value.
     * @param value The value to fill the array with.
     * @tparam T The type of the elements in the array.
     * @tparam N The size of the array.
     */
    template <typename T, std::size_t N>
    inline constexpr void fill(T *dst, T value) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < N; ++i)
                dst[i] = value;
        }

        if constexpr (simd_compatible_v<T, N>)
        {
            using pack = simd_pack<T>;
            const typename pack::type v = pack::type(value);
            for (std::size_t i = 0; i < N; i += pack::lanes)
                pack::store(dst + i, v);
        }
        else
        {
            for (std::size_t i = 0; i < N; ++i)
                dst[i] = value;
        }
    }

    /**
     * @fn load
     * @brief Copies elements from a source array to a destination array. This function will copy each element from the source array to the corresponding position in the destination array. It is marked as constexpr, allowing it to be used in compile-time contexts, and it is noexcept, indicating that it does not throw exceptions.
     * @param dst Pointer to the destination array of type T and size N where the elements will be copied to.
     * @param src Pointer to the source array of type T and size N from which the elements will be copied.
     * @tparam T The type of the elements in the arrays.
     * @tparam N The size of the arrays.
     */
    template <typename T, std::size_t N>
    inline constexpr void copy(T *dst, const T *src) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < N; ++i)
                dst[i] = src[i];
            return;
        }

        if constexpr (simd_compatible_v<T, N>)
        {
            using pack = simd_pack<T>;
            for (std::size_t i = 0; i < N; i += pack::lanes)
            {
                const auto chunk = pack::load(src + i);
                pack::store(dst + i, chunk);
            }
        }
        else
        {
            for (std::size_t i = 0; i < N; ++i)
                dst[i] = src[i];
        }
    }

    /**
     * @fn add_inplace
     * @brief Performs element-wise addition of two arrays and stores the result in the first array. This function will add each element of the second array to the corresponding element of the first array and store the result back in the first array. It is marked as constexpr, allowing it to be used in compile-time contexts, and it is noexcept, indicating that it does not throw exceptions.
     * @param a Pointer to the first array of type T and size N, which will be modified to store the result of the addition.
     * @param b Pointer to the second array of type T and size N, whose elements will be added to the corresponding elements of the first array.
     * @tparam T The type of the elements in the arrays.
     * @tparam N The size of the arrays.
     */
    template <typename T, std::size_t N>
    inline constexpr void add_inplace(T *a, const T *b) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < N; ++i)
                a[i] += b[i];
            return;
        }

        if constexpr (simd_compatible_v<T, N>)
        {
            using pack = simd_pack<T>;
            using simd_t = typename pack::type;
            for (std::size_t i = 0; i < N; i += pack::lanes)
            {
                const simd_t av = pack::load(a + i);
                const simd_t bv = pack::load(b + i);
                pack::store(a + i, av + bv);
            }
        }
        else
        {
            for (std::size_t i = 0; i < N; ++i)
                a[i] += b[i];
        }
    }

    /**
     * @fn sub_inplace
     * @brief Performs element-wise subtraction of two arrays and stores the result in the first array. This function will subtract each element of the second array from the corresponding element of the first array and store the result back in the first array. It is marked as constexpr, allowing it to be used in compile-time contexts, and it is noexcept, indicating that it does not throw exceptions.
     * @param a Pointer to the first array of type T and size N, which will be modified to store the result of the subtraction.
     * @param b Pointer to the second array of type T and size N, whose elements will be subtracted from the corresponding elements of the first array.
     * @tparam T The type of the elements in the arrays.
     * @tparam N The size of the arrays.
     */
    template <typename T, std::size_t N>
    inline constexpr void sub_inplace(T *a, const T *b) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < N; ++i)
                a[i] -= b[i];
            return;
        }

        if constexpr (simd_compatible_v<T, N>)
        {
            using pack = simd_pack<T>;
            using simd_t = typename pack::type;
            for (std::size_t i = 0; i < N; i += pack::lanes)
            {
                const simd_t av = pack::load(a + i);
                const simd_t bv = pack::load(b + i);
                pack::store(a + i, av - bv);
            }
        }
        else
        {
            for (std::size_t i = 0; i < N; ++i)
                a[i] -= b[i];
        }
    }

    /**
     * @fn mul_inplace
     * @brief Performs element-wise multiplication of two arrays and stores the result in the first array. This function will multiply each element of the first array by the corresponding element of the second array and store the result back in the first array. It is marked as constexpr, allowing it to be used in compile-time contexts, and it is noexcept, indicating that it does not throw exceptions.
     * @param a Pointer to the first array of type T and size N, which will be modified to store the result of the multiplication.
     * @param b Pointer to the second array of type T and size N, whose elements will be multiplied with the corresponding elements of the first array.
     * @tparam T The type of the elements in the arrays.
     * @tparam N The size of the arrays.
     */
    template <typename T, std::size_t N>
    inline constexpr void mul_inplace(T *a, const T *b) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < N; ++i)
                a[i] *= b[i];
            return;
        }

        if constexpr (simd_compatible_v<T, N> && (std::is_same_v<T, float> || std::is_same_v<T, double>))
        {
            using pack = simd_pack<T>;
            using simd_t = typename pack::type;
            for (std::size_t i = 0; i < N; i += pack::lanes)
            {
                const simd_t av = pack::load(a + i);
                const simd_t bv = pack::load(b + i);
                pack::store(a + i, av * bv);
            }
        }
        else
        {
            for (std::size_t i = 0; i < N; ++i)
                a[i] *= b[i];
        }
    }

    /**
     * @fn div_inplace
     * @brief Performs element-wise division of two arrays and stores the result in the first array. This function will divide each element of the first array by the corresponding element of the second array and store the result back in the first array. It is marked as constexpr, allowing it to be used in compile-time contexts, and it is noexcept, indicating that it does not throw exceptions.
     * @param a Pointer to the first array of type T and size N, which will be modified to store the result of the division.
     * @param b Pointer to the second array of type T and size N, whose elements will be used as divisors for the corresponding elements of the first array.
     * @tparam T The type of the elements in the arrays.
     * @tparam N The size of the arrays.
     */
    template <typename T, std::size_t N>
    inline constexpr void div_inplace(T *a, const T *b) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < N; ++i)
                a[i] /= b[i];
            return;
        }

        if constexpr (simd_compatible_v<T, N> && (std::is_same_v<T, float> || std::is_same_v<T, double>))
        {
            using pack = simd_pack<T>;
            using simd_t = typename pack::type;
            for (std::size_t i = 0; i < N; i += pack::lanes)
            {
                const simd_t av = pack::load(a + i);
                const simd_t bv = pack::load(b + i);
                pack::store(a + i, av / bv);
            }
        }
        else
        {
            for (std::size_t i = 0; i < N; ++i)
                a[i] /= b[i];
        }
    }

    /**
     * @fn dot
     * @brief Computes the dot product of two arrays. This function will multiply each corresponding element of the two arrays and sum the results to compute the dot product. It is marked as constexpr, allowing it to be used in compile-time contexts, and it is noexcept, indicating that it does not throw exceptions.
     * @param a Pointer to the first array of type T and size N.
     * @param b Pointer to the second array of type T and size N.
     * @return The computed dot product of the two arrays.
     * @tparam T The type of the elements in the arrays.
     * @tparam N The size of the arrays.
     */
    template <typename T, std::size_t N>
    [[nodiscard]] inline constexpr T dot(const T *a, const T *b) noexcept
    {
        if (std::is_constant_evaluated())
        {
            T sum{};
            for (std::size_t i = 0; i < N; ++i)
                sum += a[i] * b[i];
            return sum;
        }

        if constexpr (simd_compatible_v<T, N> && std::is_same_v<T, float>)
        {
            using pack = simd_pack<T>;
            using simd_t = typename pack::type;
            simd_t acc = simd_t::zero();
            for (std::size_t i = 0; i < N; i += pack::lanes)
            {
                const simd_t av = pack::load(a + i);
                const simd_t bv = pack::load(b + i);
                acc += (av * bv);
            }
            alignas(16) float tmp[4]{};
            acc.store_unaligned(tmp);
            float sum = tmp[0] + tmp[1] + tmp[2] + tmp[3];
            return static_cast<T>(sum);
        }
        else if constexpr (simd_compatible_v<T, N> && std::is_same_v<T, double>)
        {
            using pack = simd_pack<T>;
            using simd_t = typename pack::type;
            simd_t acc = simd_t::zero();
            for (std::size_t i = 0; i < N; i += pack::lanes)
            {
                const simd_t av = pack::load(a + i);
                const simd_t bv = pack::load(b + i);
                acc += (av * bv);
            }
            alignas(16) double tmp[2]{};
            acc.store_unaligned(tmp);
            double sum = tmp[0] + tmp[1];
            return static_cast<T>(sum);
        }
        else
        {
            T sum{};
            for (std::size_t i = 0; i < N; ++i)
                sum += a[i] * b[i];
            return sum;
        }
    }

    /**
     * @fn min_to
     * @brief Computes the element-wise minimum of two arrays and stores the result in a third array. This function will compare each corresponding element of the two input arrays and store the minimum value in the output array. It is marked as constexpr, allowing it to be used in compile-time contexts, and it is noexcept, indicating that it does not throw exceptions.
     * @param out Pointer to the output array of type T and size N where the minimum values will be stored.
     * @param a Pointer to the first input array of type T and size N.
     * @param b Pointer to the second input array of type T and size N.
     * @tparam T The type of the elements in the arrays.
     * @tparam N The size of the arrays.
     */
    template <typename T, std::size_t N>
    inline constexpr void min_to(T *out, const T *a, const T *b) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < N; ++i)
                out[i] = (std::min)(a[i], b[i]);
            return;
        }

        if constexpr (simd_compatible_v<T, N> && (std::is_same_v<T, float> || std::is_same_v<T, double>))
        {
            using pack = simd_pack<T>;
            using simd_t = typename pack::type;
            for (std::size_t i = 0; i < N; i += pack::lanes)
            {
                const simd_t av = pack::load(a + i);
                const simd_t bv = pack::load(b + i);
                pack::store(out + i, av.min(bv));
            }
        }
        else
        {
            for (std::size_t i = 0; i < N; ++i)
                out[i] = (std::min)(a[i], b[i]);
        }
    }

    /**
     * @fn max_to
     * @brief Computes the element-wise maximum of two arrays and stores the result in a third array. This function will compare each corresponding element of the two input arrays and store the maximum value in the output array. It is marked as constexpr, allowing it to be used in compile-time contexts, and it is noexcept, indicating that it does not throw exceptions.
     * @param out Pointer to the output array of type T and size N where the maximum values will be stored.
     * @param a Pointer to the first input array of type T and size N.
     * @param b Pointer to the second input array of type T and size N.
     * @tparam T The type of the elements in the arrays.
     * @tparam N The size of the arrays.
     */
    template <typename T, std::size_t N>
    inline constexpr void max_to(T *out, const T *a, const T *b) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < N; ++i)
                out[i] = (std::max)(a[i], b[i]);
            return;
        }

        if constexpr (simd_compatible_v<T, N> && (std::is_same_v<T, float> || std::is_same_v<T, double>))
        {
            using pack = simd_pack<T>;
            using simd_t = typename pack::type;
            for (std::size_t i = 0; i < N; i += pack::lanes)
            {
                const simd_t av = pack::load(a + i);
                const simd_t bv = pack::load(b + i);
                pack::store(out + i, av.max(bv));
            }
        }
        else
        {
            for (std::size_t i = 0; i < N; ++i)
                out[i] = (std::max)(a[i], b[i]);
        }
    }

} // namespace catalyst::math::detail
