/**
 * @file simd.hpp
 * @brief SIMD vector types and operations for 128-bit wide registers (e.g. SSE on x86, NEON on ARM).
 * @details This header defines SIMD vector types for 4 lanes of 32-bit floats (f32x4), 4 lanes of 32-bit integers (i32x4), 4 lanes of 32-bit unsigned integers (u32x4), 2 lanes of 64-bit integers (i64x2), 2 lanes of 64-bit unsigned integers (u64x2), and 2 lanes of 64-bit doubles (f64x2). Additionally, it defines mask types for boolean operations on these vectors. The actual storage and implementation of the SIMD types are platform-specific and may use intrinsics or compiler built-ins. The interface provides constructors, load/store functions, and basic arithmetic and logical operations for these SIMD types.
 * License: MIT (see LICENSE).
 */

#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @namespace catalyst::math
 * @brief The catalyst::math namespace contains all the mathematical types and functions provided by the Catalyst Math library. This includes vector and matrix types, quaternion support, angle types with user-defined literals, projection matrix utilities, transformation functions, and more. By organizing all math-related functionality within this namespace, we can avoid naming conflicts and provide a clear structure for users of the library to access the various mathematical tools they need for graphics programming and game development.
 * @details The Catalyst Math library is designed to be efficient, easy to use, and compatible with modern C++ standards. It provides a comprehensive set of mathematical utilities that are commonly used in graphics applications, making it easier for developers to perform complex mathematical operations without having to implement them from scratch. By including the appropriate headers from the Catalyst Math library, users can access all the functionality they need for their projects.
 */
namespace catalyst::math
{

    /**
     * @namespace detail
     * @brief Internal implementation details for SIMD types and operations. Not intended for public use.
     */
    namespace detail
    {
        /**
         * @struct f32x4_access
         * @brief Accessor for the internal storage of f32x4. Provides a way to access the raw bytes of an f32x4 instance for loading/storing and conversions. The actual storage is opaque and platform-specific, but this struct allows the implementation to read/write the underlying data as needed.
         */
        struct f32x4_access;
        /**
         * @struct mask32x4_access
         * @brief Accessor for the internal storage of mask32x4. Similar to f32x4_access, but for the mask type. Provides a way to access the raw bytes of a mask32x4 instance for loading/storing and conversions.
         */
        struct mask32x4_access;
        /**
         * @struct i32x4_access
         * @brief Accessor for the internal storage of i32x4. Similar to f32x4_access, but for the i32x4 integer vector type.
         */
        struct i32x4_access;
        /**
         * @struct u32x4_access
         * @brief Accessor for the internal storage of u32x4. Similar to f32x4_access, but for the u32x4 unsigned integer vector type.
         */
        struct u32x4_access;
        /**
         * @struct i64x2_access
         * @brief Accessor for the internal storage of i64x2. Similar to f32x4_access, but for the i64x2 integer vector type (2 lanes of 64-bit integer).
         */
        struct i64x2_access;
        /**
         * @struct u64x2_access
         * @brief Accessor for the internal storage of u64x2. Similar to f32x4_access, but for the u64x2 unsigned integer vector type (2 lanes of 64-bit unsigned integer).
         */
        struct u64x2_access;
        /**
         * @struct f64x2_access
         * @brief Accessor for the internal storage of f64x2. Similar to f32x4_access, but for the f64x2 double-precision floating-point vector type (2 lanes of 64-bit float).
         */
        struct f64x2_access;
        /**
         * @struct mask64x2_access
         * @brief Accessor for the internal storage of mask64x2. Similar to mask32x4_access, but for the mask64x2 type (2 lanes of boolean mask represented as 64-bit integers).
         */
        struct mask64x2_access;
    }

    /**
     * @class mask32x4
     * @brief A 128-bit wide SIMD mask type representing 4 boolean values as 32-bit integers (0 for false, 0xFFFFFFFF for true). Used for SIMD comparisons and blending operations. The actual storage is opaque and platform-specific (e.g. __m128i on x86 SSE), but the interface provides ways to construct masks, perform bitwise operations, and store/load from memory.
     */
    class alignas(16) mask32x4
    {
    public:
        /**
         * @fn mask32x4
         * @brief Default constructor initializes the mask to all false (0).
         */
        mask32x4() noexcept;

        /**
         * @fn all_false
         * @brief Returns a mask with all lanes set to false (0).
         * @return A mask32x4 with all lanes false.
         */
        [[nodiscard]] static mask32x4 all_false() noexcept;
        /**
         * @fn all_true
         * @brief Returns a mask with all lanes set to true (0xFFFFFFFF).
         * @return A mask32x4 with all lanes true.
         */
        [[nodiscard]] static mask32x4 all_true() noexcept;
        /**
         * @fn from_bits
         * @brief Creates a mask from raw 32-bit integer bits for each lane. Each lane is considered true if the corresponding bit is 0xFFFFFFFF, and false if it is 0.
         * @param b0 Bits for lane 0 (least significant lane).
         * @param b1 Bits for lane 1.
         * @param b2 Bits for lane 2.
         * @param b3 Bits for lane 3 (most significant lane).
         * @return A mask32x4 constructed from the given bits.
         */
        [[nodiscard]] static mask32x4 from_bits(std::uint32_t b0, std::uint32_t b1, std::uint32_t b2,
                                                std::uint32_t b3) noexcept;

        /**
         * @fn store_unaligned
         * @brief Stores the mask to memory as 4 32-bit integers. Each lane is stored as 0 for false or 0xFFFFFFFF for true.
         * @param ptr Pointer to an array of 4 uint32_t where the mask will be stored. The memory does not need to be aligned.
         * @note The order of lanes in memory is: lane 0 at ptr[0], lane 1 at ptr[1], lane 2 at ptr[2], lane 3 at ptr[3].
         */
        void store_unaligned(std::uint32_t *ptr) const noexcept;

        /**
         * @fn operator&
         * @brief Bitwise AND of two masks. The resulting mask has a lane set to true only if both input masks have that lane set to true.
         * @param a First mask operand.
         * @param b Second mask operand.
         * @return A new mask32x4 that is the bitwise AND of a and b.
         */
        friend mask32x4 operator&(mask32x4 a, mask32x4 b) noexcept;
        /**
         * @fn operator|
         * @brief Bitwise OR of two masks. The resulting mask has a lane set to true if either input mask has that lane set to true.
         * @param a First mask operand.
         * @param b Second mask operand.
         * @return A new mask32x4 that is the bitwise OR of a and b.
         */
        friend mask32x4 operator|(mask32x4 a, mask32x4 b) noexcept;
        /**
         * @fn operator^
         * @brief Bitwise XOR of two masks. The resulting mask has a lane set to true if exactly one of the input masks has that lane set to true.
         * @param a First mask operand.
         * @param b Second mask operand.
         * @return A new mask32x4 that is the bitwise XOR of a and b.
         */
        friend mask32x4 operator^(mask32x4 a, mask32x4 b) noexcept;
        /**
         * @fn operator~
         * @brief Bitwise NOT of a mask. The resulting mask has a lane set to true if the input mask has that lane set to false, and vice versa.
         * @param a Mask operand.
         * @return A new mask32x4 that is the bitwise NOT of a.
         */
        friend mask32x4 operator~(mask32x4 a) noexcept;

        /**
         * @fn operator&=
         * @brief Bitwise AND assignment. Updates this mask to be the bitwise AND of itself and another mask.
         * @param other Mask to AND with this mask.
         * @return Reference to this mask after the operation.
         */
        mask32x4 &operator&=(mask32x4 other) noexcept;
        /**
         * @fn operator|=
         * @brief Bitwise OR assignment. Updates this mask to be the bitwise OR of itself and another mask.
         * @param other Mask to OR with this mask.
         * @return Reference to this mask after the operation.
         */
        mask32x4 &operator|=(mask32x4 other) noexcept;
        /**
         * @fn operator^=
         * @brief Bitwise XOR assignment. Updates this mask to be the bitwise XOR of itself and another mask.
         * @param other Mask to XOR with this mask.
         * @return Reference to this mask after the operation.
         */
        mask32x4 &operator^=(mask32x4 other) noexcept;

    private:
        friend struct detail::mask32x4_access;
        /**
         * @brief Opaque storage for 4x 32-bit integer mask values. The implementation interprets this memory as the platform's native SIMD register for masks (e.g. __m128i on x86 SSE), but the interface abstracts away the details.
         */
        alignas(16) std::byte storage_[16];
    };

    static_assert(sizeof(mask32x4) == 16, "catalyst::math::mask32x4 must be 16 bytes");
    static_assert(alignof(mask32x4) == 16, "catalyst::math::mask32x4 must be 16-byte aligned");

    /**
     * @class f32x4
     * @brief A 128-bit wide SIMD vector type representing 4 lanes of 32-bit floating-point values. Provides basic arithmetic operations, comparisons, and utility functions for working with 4-wide vectors of floats. The actual storage is opaque and platform-specific (e.g. __m128 on x86 SSE), but the interface allows for constructing vectors, performing element-wise operations, and loading/storing from memory.
     */
    class alignas(16) f32x4
    {
    public:
        /**
         * @fn f32x4
         * @brief Default constructor initializes the vector to all zeros.
         */
        f32x4() noexcept;

        /**
         * @fn zero
         * @brief Returns a vector with all lanes set to zero.
         */
        [[nodiscard]] static f32x4 zero() noexcept;
        /**
         * @fn set
         * @brief Creates a vector from 4 individual float values for each lane.
         * @param x0 Value for lane 0 (least significant lane).
         * @param x1 Value for lane 1.
         * @param x2 Value for lane 2.
         * @param x3 Value for lane 3 (most significant lane).
         * @return A f32x4 vector constructed from the given values.
         */
        [[nodiscard]] static f32x4 set(float x0, float x1, float x2, float x3) noexcept;
        /**
         * @fn splat
         * @brief Creates a vector with all lanes set to the same float value.
         * @param x The float value to replicate across all lanes.
         * @return A f32x4 vector with all lanes set to x.
         */
        [[nodiscard]] static f32x4 splat(float x) noexcept;

        /**
         * @fn load_aligned
         * @brief Loads a vector from memory with the assumption that the pointer is 16-byte aligned. The memory should contain 4 contiguous float values that will be loaded into the vector lanes.
         * @param ptr Pointer to an array of 4 float values to load. Must be 16-byte aligned.
         * @return A f32x4 vector loaded from the given memory.
         */
        [[nodiscard]] static f32x4 load_aligned(const float *ptr) noexcept;
        /**
         * @fn load_unaligned
         * @brief Loads a vector from memory without any alignment requirements. The memory should contain 4 contiguous float values that will be loaded into the vector lanes.
         * @param ptr Pointer to an array of 4 float values to load. Does not need to be aligned.
         * @return A f32x4 vector loaded from the given memory.
         */
        [[nodiscard]] static f32x4 load_unaligned(const float *ptr) noexcept;

        /**
         * @fn store_aligned
         * @brief Stores the vector to memory with the assumption that the pointer is 16-byte aligned. The vector's 4 lanes will be stored as contiguous float values in memory.
         * @param ptr Pointer to an array of 4 float values where the vector will be stored. Must be 16-byte aligned.
         */
        void store_aligned(float *ptr) const noexcept;
        /**
         * @fn store_unaligned
         * @brief Stores the vector to memory without any alignment requirements. The vector's 4 lanes will be stored as contiguous float values in memory.
         * @param ptr Pointer to an array of 4 float values where the vector will be stored. Does not need to be aligned.
         */
        void store_unaligned(float *ptr) const noexcept;

        /**
         * @fn operator+
         * @brief Element-wise addition of two vectors. Each lane of the result is the sum of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new f32x4 vector that is the element-wise sum of a and b.
         */
        friend f32x4 operator+(f32x4 a, f32x4 b) noexcept;
        /**
         * @fn operator-
         * @brief Element-wise subtraction of two vectors. Each lane of the result is the difference of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new f32x4 vector that is the element-wise difference of a and b.
         */
        friend f32x4 operator-(f32x4 a, f32x4 b) noexcept;
        /**
         * @fn operator*
         * @brief Element-wise multiplication of two vectors. Each lane of the result is the product of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new f32x4 vector that is the element-wise product of a and b.
         */
        friend f32x4 operator*(f32x4 a, f32x4 b) noexcept;
        /**
         * @fn operator/
         * @brief Element-wise division of two vectors. Each lane of the result is the quotient of the corresponding lanes of the input vectors.
         * @param a First vector operand (numerator).
         * @param b Second vector operand (denominator).
         * @return A new f32x4 vector that is the element-wise quotient of a and b.
         */
        friend f32x4 operator/(f32x4 a, f32x4 b) noexcept;

        /**
         * @fn operator+=
         * @brief Element-wise addition assignment. Updates this vector to be the element-wise sum of itself and another vector.
         * @param other Vector to add to this vector.
         * @return Reference to this vector after the operation.
         */
        f32x4 &operator+=(f32x4 other) noexcept;
        /**
         * @fn operator-=
         * @brief Element-wise subtraction assignment. Updates this vector to be the element-wise difference of itself and another vector.
         * @param other Vector to subtract from this vector.
         * @return Reference to this vector after the operation.
         */
        f32x4 &operator-=(f32x4 other) noexcept;
        /**
         * @fn operator*=
         * @brief Element-wise multiplication assignment. Updates this vector to be the element-wise product of itself and another vector.
         * @param other Vector to multiply with this vector.
         * @return Reference to this vector after the operation.
         */
        f32x4 &operator*=(f32x4 other) noexcept;
        /**
         * @fn operator/=
         * @brief Element-wise division assignment. Updates this vector to be the element-wise quotient of itself and another vector.
         * @param other Vector to divide this vector by.
         * @return Reference to this vector after the operation.
         */
        f32x4 &operator/=(f32x4 other) noexcept;

        /**
         * @fn abs
         * @brief Element-wise absolute value of the vector. Each lane of the result is the absolute value of the corresponding lane of this vector.
         * @return A new f32x4 vector where each lane is the absolute value of the corresponding lane of this vector.
         */
        [[nodiscard]] f32x4 abs() const noexcept;

        /**
         * @fn min
         * @brief Element-wise minimum of this vector and another vector. Each lane of the result is the minimum of the corresponding lanes of the two vectors.
         * @param other The other vector to compare with this vector.
         */
        [[nodiscard]] f32x4 min() const noexcept;
        /**
         * @fn max
         * @brief Element-wise maximum of this vector and another vector. Each lane of the result is the maximum of the corresponding lanes of the two vectors.
         * @param other The other vector to compare with this vector.
         */
        [[nodiscard]] f32x4 max() const noexcept;

        /**
         * @fn mask
         * @brief Creates a mask from this vector where each lane is considered true if the corresponding lane of this vector is less than zero, and false otherwise. The resulting mask can be used for conditional selection or blending operations.
         * @param other The vector to compare with this vector for generating the mask.
         * @return A mask32x4 where each lane is true if the corresponding lane of this vector is < 0, and false otherwise.
         */
        [[nodiscard]] f32x4 min(f32x4 other) const noexcept;
        /**
         * @fn max
         * @brief Element-wise maximum of this vector and another vector. Each lane of the result is the maximum of the corresponding lanes of the two vectors.
         * @param other The other vector to compare with this vector.
         * @return A new f32x4 vector where each lane is the maximum of the corresponding lanes of this vector and the other vector.
         */
        [[nodiscard]] f32x4 max(f32x4 other) const noexcept;

        /**
         * @fn mask
         * @brief Creates a mask from this vector where each lane is considered true if the corresponding lane of this vector is less than zero, and false otherwise. The resulting mask can be used for conditional selection or blending operations.
         * @return A mask32x4 where each lane is true if the corresponding lane of this vector is < 0, and false otherwise.
         */
        [[nodiscard]] mask32x4 mask() const noexcept;

        /**
         * @fn sqrt
         * @brief Element-wise square root of the vector. Each lane of the result is the square root of the corresponding lane of this vector.
         * @return A new f32x4 vector where each lane is the square root of the corresponding lane of this vector.
         * @note The behavior is undefined if any lane of this vector is negative. If a lane is zero, the result for that lane will be zero. If a lane is positive infinity, the result for that lane will be positive infinity. If a lane is NaN, the result for that lane will be NaN.
         */
        [[nodiscard]] f32x4 sqrt() const noexcept;
        /**
         * @fn rsqrt
         * @brief Element-wise reciprocal square root of the vector. Each lane of the result is 1.0 divided by the square root of the corresponding lane of this vector.
         * @return A new f32x4 vector where each lane is the reciprocal square root of the corresponding lane of this vector.
         * @note The behavior is undefined if any lane of this vector is negative or zero. If a lane is positive infinity, the result for that lane will be zero. If a lane is NaN, the result for that lane will be NaN.
         */
        [[nodiscard]] f32x4 rsqrt() const noexcept;
        /**
         * @fn reciprocal
         * @brief Element-wise reciprocal of the vector. Each lane of the result is 1.0 divided by the corresponding lane of this vector.
         * @return A new f32x4 vector where each lane is the reciprocal of the corresponding lane of this vector.
         * @note The behavior is undefined if any lane of this vector is zero. If a lane is positive infinity, the result for that lane will be zero. If a lane is negative infinity, the result for that lane will be negative zero. If a lane is NaN, the result for that lane will be NaN.
         */
        [[nodiscard]] f32x4 reciprocal() const noexcept;

    private:
        friend struct detail::f32x4_access;

        /**
         * @brief Opaque storage for 4x 32-bit float values. The implementation interprets this memory as the platform's native SIMD register for 4-wide floats (e.g. __m128 on x86 SSE), but the interface abstracts away the details.
         * @details The actual layout and representation of the data in storage_ is platform-specific and may use SIMD intrinsics for efficient access. The interface provides methods to load/store from memory and perform operations without exposing the internal representation.
         */
        alignas(16) std::byte storage_[16];
    };

    static_assert(sizeof(f32x4) == 16, "catalyst::math::f32x4 must be 16 bytes");
    static_assert(alignof(f32x4) == 16, "catalyst::math::f32x4 must be 16-byte aligned");

    /**
     * @class i32x4
     * @brief A 128-bit wide SIMD vector type representing 4 lanes of 32-bit signed integer values. Provides basic arithmetic operations, bitwise operations, and utility functions for working with 4-wide vectors of integers. The actual storage is opaque and platform-specific (e.g. __m128i on x86 SSE), but the interface allows for constructing vectors, performing element-wise operations, and loading/storing from memory.
     */
    class alignas(16) i32x4
    {
    public:
        /**
         * @fn i32x4
         * @brief Default constructor initializes the vector to all zeros.
         */
        i32x4() noexcept;

        /**
         * @fn zero
         * @brief Returns a vector with all lanes set to zero.
         */
        [[nodiscard]] static i32x4 zero() noexcept;
        /**
         * @fn set
         * @brief Creates a vector from 4 individual int32_t values for each lane.
         * @param x0 Value for lane 0 (least significant lane).
         * @param x1 Value for lane 1.
         * @param x2 Value for lane 2.
         * @param x3 Value for lane 3 (most significant lane).
         * @return A i32x4 vector constructed from the given values.
         */
        [[nodiscard]] static i32x4 set(std::int32_t x0, std::int32_t x1, std::int32_t x2, std::int32_t x3) noexcept;
        /**
         * @fn splat
         * @brief Creates a vector with all lanes set to the same int32_t value.
         * @param x The int32_t value to replicate across all lanes.
         * @return A i32x4 vector with all lanes set to x.
         */
        [[nodiscard]] static i32x4 splat(std::int32_t x) noexcept;

        /**
         * @fn load_aligned
         * @brief Loads a vector from memory with the assumption that the pointer is 16-byte aligned. The memory should contain 4 contiguous int32_t values that will be loaded into the vector lanes.
         * @param ptr Pointer to an array of 4 int32_t values to load. Must be 16-byte aligned.
         * @return A i32x4 vector loaded from the given memory.
         */
        [[nodiscard]] static i32x4 load_aligned(const std::int32_t *ptr) noexcept;
        /**
         * @fn load_unaligned
         * @brief Loads a vector from memory without any alignment requirements. The memory should contain 4 contiguous int32_t values that will be loaded into the vector lanes.
         * @param ptr Pointer to an array of 4 int32_t values to load. Does not need to be aligned.
         * @return A i32x4 vector loaded from the given memory.
         */
        [[nodiscard]] static i32x4 load_unaligned(const std::int32_t *ptr) noexcept;

        /**
         * @fn store_aligned
         * @brief Stores the vector to memory with the assumption that the pointer is 16-byte aligned. The vector's 4 lanes will be stored as contiguous int32_t values in memory.
         * @param ptr Pointer to an array of 4 int32_t values where the vector will be stored. Must be 16-byte aligned.
         */
        void store_aligned(std::int32_t *ptr) const noexcept;
        /**
         * @fn store_unaligned
         * @brief Stores the vector to memory without any alignment requirements. The vector's 4 lanes will be stored as contiguous int32_t values in memory.
         * @param ptr Pointer to an array of 4 int32_t values where the vector will be stored. Does not need to be aligned.
         */
        void store_unaligned(std::int32_t *ptr) const noexcept;

        /**
         * @fn operator+
         * @brief Element-wise addition of two vectors. Each lane of the result is the sum of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new i32x4 vector that is the element-wise sum of a and b.
         */
        friend i32x4 operator+(i32x4 a, i32x4 b) noexcept;
        /**
         * @fn operator-
         * @brief Element-wise subtraction of two vectors. Each lane of the result is the difference of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new i32x4 vector that is the element-wise difference of a and b.
         */
        friend i32x4 operator-(i32x4 a, i32x4 b) noexcept;

        /**
         * @fn operator&
         * @brief Bitwise AND of two vectors. Each lane of the result is the bitwise AND of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new i32x4 vector that is the bitwise AND of a and b.
         */
        friend i32x4 operator&(i32x4 a, i32x4 b) noexcept;
        /**
         * @fn operator|
         * @brief Bitwise OR of two vectors. Each lane of the result is the bitwise OR of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new i32x4 vector that is the bitwise OR of a and b.
         */
        friend i32x4 operator|(i32x4 a, i32x4 b) noexcept;
        /**
         * @fn operator^
         * @brief Bitwise XOR of two vectors. Each lane of the result is the bitwise XOR of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new i32x4 vector that is the bitwise XOR of a and b.
         */
        friend i32x4 operator^(i32x4 a, i32x4 b) noexcept;
        /**
         * @fn operator~
         * @brief Bitwise NOT of a vector. Each lane of the result is the bitwise NOT of the corresponding lane of the input vector.
         * @param a Vector operand.
         * @return A new i32x4 vector that is the bitwise NOT of a.
         */
        friend i32x4 operator~(i32x4 a) noexcept;

        /**
         * @fn operator+=
         * @brief Element-wise addition assignment. Updates this vector to be the element-wise sum of itself and another vector.
         * @param other Vector to add to this vector.
         * @return Reference to this vector after the operation.
         */
        i32x4 &operator+=(i32x4 other) noexcept;
        /**
         * @fn operator-=
         * @brief Element-wise subtraction assignment. Updates this vector to be the element-wise difference of itself and another vector.
         * @param other Vector to subtract from this vector.
         * @return Reference to this vector after the operation.
         */
        i32x4 &operator-=(i32x4 other) noexcept;
        /**
         * @fn operator&=
         * @brief Bitwise AND assignment. Updates this vector to be the bitwise AND of itself and another vector.
         * @param other Vector to AND with this vector.
         * @return Reference to this vector after the operation.
         */
        i32x4 &operator&=(i32x4 other) noexcept;
        /**
         * @fn operator|=
         * @brief Bitwise OR assignment. Updates this vector to be the bitwise OR of itself and another vector.
         * @param other Vector to OR with this vector.
         * @return Reference to this vector after the operation.
         */
        i32x4 &operator|=(i32x4 other) noexcept;
        /**
         * @fn operator^=
         * @brief Bitwise XOR assignment. Updates this vector to be the bitwise XOR of itself and another vector.
         * @param other Vector to XOR with this vector.
         * @return Reference to this vector after the operation.
         */
        i32x4 &operator^=(i32x4 other) noexcept;

    private:
        friend struct detail::i32x4_access;
        /**
         * @brief Opaque storage for 4x 32-bit integer values. The implementation interprets this memory as the platform's native SIMD register for 4-wide integers (e.g. __m128i on x86 SSE), but the interface abstracts away the details.
         * @details The actual layout and representation of the data in storage_ is platform-specific and may use SIMD intrinsics for efficient access. The interface provides methods to load/store from memory and perform operations without exposing the internal representation.
         */
        alignas(16) std::byte storage_[16];
    };

    static_assert(sizeof(i32x4) == 16, "catalyst::math::i32x4 must be 16 bytes");
    static_assert(alignof(i32x4) == 16, "catalyst::math::i32x4 must be 16-byte aligned");

    /**
     * @class u32x4
     * @brief A 128-bit wide SIMD vector type representing 4 lanes of 32-bit unsigned integer values. Provides basic arithmetic operations, bitwise operations, and utility functions for working with 4-wide vectors of unsigned integers. The actual storage is opaque and platform-specific (e.g. __m128i on x86 SSE), but the interface allows for constructing vectors, performing element-wise operations, and loading/storing from memory.
     */
    class alignas(16) u32x4
    {
    public:
        /**
         * @fn u32x4
         * @brief Default constructor initializes the vector to all zeros.
         */
        u32x4() noexcept;

        /**
         * @fn zero
         * @brief Returns a vector with all lanes set to zero.
         */
        [[nodiscard]] static u32x4 zero() noexcept;
        /**
         * @fn set
         * @brief Creates a vector from 4 individual uint32_t values for each lane.
         * @param x0 Value for lane 0 (least significant lane).
         * @param x1 Value for lane 1.
         * @param x2 Value for lane 2.
         * @param x3 Value for lane 3 (most significant lane).
         * @return A u32x4 vector constructed from the given values.
         */
        [[nodiscard]] static u32x4 set(std::uint32_t x0, std::uint32_t x1, std::uint32_t x2, std::uint32_t x3) noexcept;
        /**
         * @fn splat
         * @brief Creates a vector with all lanes set to the same uint32_t value.
         * @param x The uint32_t value to replicate across all lanes.
         * @return A u32x4 vector with all lanes set to x.
         */
        [[nodiscard]] static u32x4 splat(std::uint32_t x) noexcept;

        /**
         * @fn load_aligned
         * @brief Loads a vector from memory with the assumption that the pointer is 16-byte aligned. The memory should contain 4 contiguous uint32_t values that will be loaded into the vector lanes.
         * @param ptr Pointer to an array of 4 uint32_t values to load. Must be 16-byte aligned.
         * @return A u32x4 vector loaded from the given memory.
         */
        [[nodiscard]] static u32x4 load_aligned(const std::uint32_t *ptr) noexcept;
        /**
         * @fn load_unaligned
         * @brief Loads a vector from memory without any alignment requirements. The memory should contain 4 contiguous uint32_t values that will be loaded into the vector lanes.
         * @param ptr Pointer to an array of 4 uint32_t values to load. Does not need to be aligned.
         * @return A u32x4 vector loaded from the given memory.
         */
        [[nodiscard]] static u32x4 load_unaligned(const std::uint32_t *ptr) noexcept;

        /**
         * @fn store_aligned
         * @brief Stores the vector to memory with the assumption that the pointer is 16-byte aligned. The vector's 4 lanes will be stored as contiguous uint32_t values in memory.
         * @param ptr Pointer to an array of 4 uint32_t values where the vector will be stored. Must be 16-byte aligned.
         */
        void store_aligned(std::uint32_t *ptr) const noexcept;
        /**
         * @fn store_unaligned
         * @brief Stores the vector to memory without any alignment requirements. The vector's 4 lanes will be stored as contiguous uint32_t values in memory.
         * @param ptr Pointer to an array of 4 uint32_t values where the vector will be stored. Does not need to be aligned.
         */
        void store_unaligned(std::uint32_t *ptr) const noexcept;

        /**
         * @fn operator+
         * @brief Element-wise addition of two vectors. Each lane of the result is the sum of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new u32x4 vector that is the element-wise sum of a and b.
         */
        friend u32x4 operator+(u32x4 a, u32x4 b) noexcept;
        /**
         * @fn operator-
         * @brief Element-wise subtraction of two vectors. Each lane of the result is the difference of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new u32x4 vector that is the element-wise difference of a and b.
         */
        friend u32x4 operator-(u32x4 a, u32x4 b) noexcept;

        /**
         * @fn operator&
         * @brief Bitwise AND of two vectors. Each lane of the result is the bitwise AND of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new u32x4 vector that is the bitwise AND of a and b.
         */
        friend u32x4 operator&(u32x4 a, u32x4 b) noexcept;
        /**
         * @fn operator|
         * @brief Bitwise OR of two vectors. Each lane of the result is the bitwise OR of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new u32x4 vector that is the bitwise OR of a and b.
         */
        friend u32x4 operator|(u32x4 a, u32x4 b) noexcept;
        /**
         * @fn operator^
         * @brief Bitwise XOR of two vectors. Each lane of the result is the bitwise XOR of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new u32x4 vector that is the bitwise XOR of a and b.
         */
        friend u32x4 operator^(u32x4 a, u32x4 b) noexcept;
        /**
         * @fn operator~
         * @brief Bitwise NOT of a vector. Each lane of the result is the bitwise NOT of the corresponding lane of the input vector.
         * @param a Vector operand.
         * @return A new u32x4 vector that is the bitwise NOT of a.
         */
        friend u32x4 operator~(u32x4 a) noexcept;

        /**
         * @fn operator+=
         * @brief Element-wise addition assignment. Updates this vector to be the element-wise sum of itself and another vector.
         * @param other Vector to add to this vector.
         * @return Reference to this vector after the operation.
         */
        u32x4 &operator+=(u32x4 other) noexcept;
        /**
         * @fn operator-=
         * @brief Element-wise subtraction assignment. Updates this vector to be the element-wise difference of itself and another vector.
         * @param other Vector to subtract from this vector.
         * @return Reference to this vector after the operation.
         */
        u32x4 &operator-=(u32x4 other) noexcept;
        /**
         * @fn operator&=
         * @brief Bitwise AND assignment. Updates this vector to be the bitwise AND of itself and another vector.
         * @param other Vector to AND with this vector.
         * @return Reference to this vector after the operation.
         */
        u32x4 &operator&=(u32x4 other) noexcept;
        /**
         * @fn operator|=
         * @brief Bitwise OR assignment. Updates this vector to be the bitwise OR of itself and another vector.
         * @param other Vector to OR with this vector.
         * @return Reference to this vector after the operation.
         */
        u32x4 &operator|=(u32x4 other) noexcept;
        /**
         * @fn operator^=
         * @brief Bitwise XOR assignment. Updates this vector to be the bitwise XOR of itself and another vector.
         * @param other Vector to XOR with this vector.
         * @return Reference to this vector after the operation.
         */
        u32x4 &operator^=(u32x4 other) noexcept;

    private:
        friend struct detail::u32x4_access;

        /**
         * @brief Opaque storage for 4x 32-bit unsigned integer values. The implementation interprets this memory as the platform's native SIMD register for 4-wide integers (e.g. __m128i on x86 SSE), but the interface abstracts away the details.
         * @details The actual layout and representation of the data in storage_ is platform-specific and may use SIMD intrinsics for efficient access. The interface provides methods to load/store from memory and perform operations without exposing the internal representation.
         */
        alignas(16) std::byte storage_[16];
    };

    static_assert(sizeof(u32x4) == 16, "catalyst::math::u32x4 must be 16 bytes");
    static_assert(alignof(u32x4) == 16, "catalyst::math::u32x4 must be 16-byte aligned");

    /**
     * @class f64x2
     * @brief A 128-bit wide SIMD vector type representing 2 lanes of 64-bit double-precision floating-point values. Provides basic arithmetic operations and utility functions for working with 2-wide vectors of doubles. The actual storage is opaque and platform-specific (e.g. __m128d on x86 SSE2), but the interface allows for constructing vectors, performing element-wise operations, and loading/storing from memory.
     */
    class alignas(16) f64x2
    {
    public:
        /**
         * @fn f64x2
         * @brief Default constructor initializes the vector to all zeros.
         */
        f64x2() noexcept;

        /**
         * @fn zero
         * @brief Returns a vector with all lanes set to zero.
         */
        [[nodiscard]] static f64x2 zero() noexcept;
        /**
         * @fn set
         * @brief Creates a vector from 2 individual double values for each lane.
         * @param x0 Value for lane 0 (least significant lane).
         * @param x1 Value for lane 1 (most significant lane).
         * @return A f64x2 vector constructed from the given values.
         */
        [[nodiscard]] static f64x2 set(double x0, double x1) noexcept;
        /**
         * @fn splat
         * @brief Creates a vector with all lanes set to the same double value.
         * @param x The double value to replicate across both lanes.
         * @return A f64x2 vector with all lanes set to x.
         */
        [[nodiscard]] static f64x2 splat(double x) noexcept;

        /**
         * @fn load_aligned
         * @brief Loads a vector from memory with the assumption that the pointer is 16-byte aligned. The memory should contain 2 contiguous double values that will be loaded into the vector lanes.
         * @param ptr Pointer to an array of 2 double values to load. Must be 16-byte aligned.
         * @return A f64x2 vector loaded from the given memory.
         */
        [[nodiscard]] static f64x2 load_aligned(const double *ptr) noexcept;
        /**
         * @fn load_unaligned
         * @brief Loads a vector from memory without any alignment requirements. The memory should contain 2 contiguous double values that will be loaded into the vector lanes.
         * @param ptr Pointer to an array of 2 double values to load. Does not need to be aligned.
         * @return A f64x2 vector loaded from the given memory.
         */
        [[nodiscard]] static f64x2 load_unaligned(const double *ptr) noexcept;

        /**
         * @fn store_aligned
         * @brief Stores the vector to memory with the assumption that the pointer is 16-byte aligned. The vector's 2 lanes will be stored as contiguous double values in memory.
         * @param ptr Pointer to an array of 2 double values where the vector will be stored. Must be 16-byte aligned.
         */
        void store_aligned(double *ptr) const noexcept;
        /**
         * @fn store_unaligned
         * @brief Stores the vector to memory without any alignment requirements. The vector's 2 lanes will be stored as contiguous double values in memory.
         * @param ptr Pointer to an array of 2 double values where the vector will be stored. Does not need to be aligned.
         */
        void store_unaligned(double *ptr) const noexcept;

        /**
         * @fn operator+
         * @brief Element-wise addition of two vectors. Each lane of the result is the sum of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new f64x2 vector that is the element-wise sum of a and b.
         */
        friend f64x2 operator+(f64x2 a, f64x2 b) noexcept;
        /**
         * @fn operator-
         * @brief Element-wise subtraction of two vectors. Each lane of the result is the difference of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new f64x2 vector that is the element-wise difference of a and b.
         */
        friend f64x2 operator-(f64x2 a, f64x2 b) noexcept;
        /**
         * @fn operator*
         * @brief Element-wise multiplication of two vectors. Each lane of the result is the product of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new f64x2 vector that is the element-wise product of a and b.
         */
        friend f64x2 operator*(f64x2 a, f64x2 b) noexcept;
        /**
         * @fn operator/
         * @brief Element-wise division of two vectors. Each lane of the result is the quotient of the corresponding lanes of the input vectors.
         * @param a First vector operand (numerator).
         * @param b Second vector operand (denominator).
         * @return A new f64x2 vector that is the element-wise quotient of a and b.
         */
        friend f64x2 operator/(f64x2 a, f64x2 b) noexcept;

        /**
         * @fn operator+=
         * @brief Element-wise addition assignment. Updates this vector to be the element-wise sum of itself and another vector.
         * @param other Vector to add to this vector.
         * @return Reference to this vector after the operation.
         */
        f64x2 &operator+=(f64x2 other) noexcept;
        /**
         * @fn operator-=
         * @brief Element-wise subtraction assignment. Updates this vector to be the element-wise difference of itself and another vector.
         * @param other Vector to subtract from this vector.
         * @return Reference to this vector after the operation.
         */
        f64x2 &operator-=(f64x2 other) noexcept;
        /**
         * @fn operator*=
         * @brief Element-wise multiplication assignment. Updates this vector to be the element-wise product of itself and another vector.
         * @param other Vector to multiply with this vector.
         * @return Reference to this vector after the operation.
         */
        f64x2 &operator*=(f64x2 other) noexcept;
        /**
         * @fn operator/=
         * @brief Element-wise division assignment. Updates this vector to be the element-wise quotient of itself and another vector.
         * @param other Vector to divide this vector by.
         * @return Reference to this vector after the operation.
         */
        f64x2 &operator/=(f64x2 other) noexcept;

        /**
         * @fn abs
         * @brief Element-wise absolute value of the vector. Each lane of the result is the absolute value of the corresponding lane of this vector.
         * @return A new f64x2 vector where each lane is the absolute value of the corresponding lane of this vector.
         */
        [[nodiscard]] f64x2 abs() const noexcept;
        /**
         * @fn min
         * @brief Element-wise minimum of this vector and another vector. Each lane of the result is the minimum of the corresponding lanes of the two vectors.
         * @param other The other vector to compare with this vector.
         * @return A new f64x2 vector where each lane is the minimum of the corresponding lanes of this vector and the other vector.
         */
        [[nodiscard]] f64x2 min(f64x2 other) const noexcept;
        /**
         * @fn max
         * @brief Element-wise maximum of this vector and another vector. Each lane of the result is the maximum of the corresponding lanes of the two vectors.
         * @param other The other vector to compare with this vector.
         * @return A new f64x2 vector where each lane is the maximum of the corresponding lanes of this vector and the other vector.
         */
        [[nodiscard]] f64x2 max(f64x2 other) const noexcept;
        /**
         * @fn sqrt
         * @brief Element-wise square root of the vector. Each lane of the result is the square root of the corresponding lane of this vector.
         * @return A new f64x2 vector where each lane is the square root of the corresponding lane of this vector.
         * @note The behavior is undefined if any lane of this vector is negative. If a lane is zero, the result for that lane will be zero. If a lane is positive infinity, the result for that lane will be positive infinity. If a lane is NaN, the result for that lane will be NaN.
         */
        [[nodiscard]] f64x2 sqrt() const noexcept;
        /**
         * @fn rsqrt
         * @brief Element-wise reciprocal square root of the vector. Each lane of the result is 1.0 divided by the square root of the corresponding lane of this vector.
         * @return A new f64x2 vector where each lane is the reciprocal square root of the corresponding lane of this vector.
         * @note The behavior is undefined if any lane of this vector is negative or zero. If a lane is positive infinity, the result for that lane will be zero. If a lane is NaN, the result for that lane will be NaN.
         */
        [[nodiscard]] f64x2 rsqrt() const noexcept;
        /**
         * @fn reciprocal
         * @brief Element-wise reciprocal of the vector. Each lane of the result is 1.0 divided by the corresponding lane of this vector.
         * @return A new f64x2 vector where each lane is the reciprocal of the corresponding lane of this vector.
         * @note The behavior is undefined if any lane of this vector is zero. If a lane is positive infinity, the result for that lane will be zero. If a lane is negative infinity, the result for that lane will be negative zero. If a lane is NaN, the result for that lane will be NaN.
         */
        [[nodiscard]] f64x2 reciprocal() const noexcept;

    private:
        friend struct detail::f64x2_access;

        /**
         * @brief Opaque storage for 2x 64-bit double values. The implementation interprets this memory as the platform's native SIMD register for 2-wide doubles (e.g. __m128d on x86 SSE2), but the interface abstracts away the details.
         * @details The actual layout and representation of the data in storage_ is platform-specific and may use SIMD intrinsics for efficient access. The interface provides methods to load/store from memory and perform operations without exposing the internal representation.
         */
        alignas(16) std::byte storage_[16];
    };

    static_assert(sizeof(f64x2) == 16, "catalyst::math::f64x2 must be 16 bytes");
    static_assert(alignof(f64x2) == 16, "catalyst::math::f64x2 must be 16-byte aligned");

    /**
     * @class mask64x2
     * @brief A 128-bit wide SIMD vector type representing 2 lanes of 64-bit mask values. Each lane is typically used to represent a boolean condition (e.g. all bits set for true, all bits clear for false) in SIMD operations. Provides bitwise operations and utility functions for working with 2-wide vectors of masks. The actual storage is opaque and platform-specific, but the interface allows for constructing mask vectors, performing bitwise operations, and loading/storing from memory.
     */
    class alignas(16) mask64x2
    {
    public:
        /**
         * @fn mask64x2
         * @brief Default constructor initializes the mask vector to all false (all bits clear).
         */
        mask64x2() noexcept;

        /**
         * @fn all_false
         * @brief Returns a mask vector with all lanes set to false (all bits clear).
         * @return A mask64x2 vector where all lanes are false.
         */
        [[nodiscard]] static mask64x2 all_false() noexcept;
        /**
         * @fn all_true
         * @brief Returns a mask vector with all lanes set to true (all bits set).
         * @return A mask64x2 vector where all lanes are true.
         */
        [[nodiscard]] static mask64x2 all_true() noexcept;
        /**
         * @fn set
         * @brief Creates a mask vector from 2 individual uint64_t values for each lane. Each value should be either all bits clear (0) for false or all bits set (e.g. 0xFFFFFFFFFFFFFFFF) for true.
         * @param b0 Value for lane 0 (least significant lane).
         * @param b1 Value for lane 1 (most significant lane).
         * @return A mask64x2 vector constructed from the given values.
         */
        [[nodiscard]] static mask64x2 from_bits(std::uint64_t b0, std::uint64_t b1) noexcept;

        /**
         * @fn store_unaligned
         * @brief Stores the mask vector to memory without any alignment requirements. The vector's 2 lanes will be stored as contiguous uint64_t values in memory, where each value represents the bits of the corresponding lane of the mask.
         * @param ptr Pointer to an array of 2 uint64_t values where the mask vector will be stored. Does not need to be aligned.
         * @note This function is provided for debugging and interoperability purposes, as mask vectors are often used in conjunction with other SIMD operations. The actual bit patterns stored may depend on the platform's representation of mask values (e.g. all bits set for true, all bits clear for false).
         */
        void store_unaligned(std::uint64_t *ptr) const noexcept;

        /**
         * @fn operator&
         * @brief Bitwise AND of two mask vectors. Each lane of the result is the bitwise AND of the corresponding lanes of the input mask vectors.
         * @param a First mask vector operand.
         * @param b Second mask vector operand.
         * @return A new mask64x2 vector that is the bitwise AND of a and b.
         */
        friend mask64x2 operator&(mask64x2 a, mask64x2 b) noexcept;
        /**
         * @fn operator|
         * @brief Bitwise OR of two mask vectors. Each lane of the result is the bitwise OR of the corresponding lanes of the input mask vectors.
         * @param a First mask vector operand.
         * @param b Second mask vector operand.
         * @return A new mask64x2 vector that is the bitwise OR of a and b.
         */
        friend mask64x2 operator|(mask64x2 a, mask64x2 b) noexcept;
        /**
         * @fn operator^
         * @brief Bitwise XOR of two mask vectors. Each lane of the result is the bitwise XOR of the corresponding lanes of the input mask vectors.
         * @param a First mask vector operand.
         * @param b Second mask vector operand.
         * @return A new mask64x2 vector that is the bitwise XOR of a and b.
         */
        friend mask64x2 operator^(mask64x2 a, mask64x2 b) noexcept;
        /**
         * @fn operator~
         * @brief Bitwise NOT of a mask vector. Each lane of the result is the bitwise NOT of the corresponding lane of the input mask vector.
         * @param a Mask vector operand.
         * @return A new mask64x2 vector that is the bitwise NOT of a.
         */
        friend mask64x2 operator~(mask64x2 a) noexcept;

        /**
         * @fn operator&=
         * @brief Bitwise AND assignment. Updates this mask vector to be the bitwise AND of itself and another mask vector.
         * @param other Mask vector to AND with this mask vector.
         * @return Reference to this mask vector after the operation.
         */
        mask64x2 &operator&=(mask64x2 other) noexcept;
        /**
         * @fn operator|=
         * @brief Bitwise OR assignment. Updates this mask vector to be the bitwise OR of itself and another mask vector.
         * @param other Mask vector to OR with this mask vector.
         * @return Reference to this mask vector after the operation.
         */
        mask64x2 &operator|=(mask64x2 other) noexcept;
        /**
         * @fn operator^=
         * @brief Bitwise XOR assignment. Updates this mask vector to be the bitwise XOR of itself and another mask vector.
         * @param other Mask vector to XOR with this mask vector.
         * @return Reference to this mask vector after the operation.
         */
        mask64x2 &operator^=(mask64x2 other) noexcept;

    private:
        friend struct detail::mask64x2_access;

        /**
         * @brief Opaque storage for 2x 64-bit mask values. The implementation interprets this memory as the platform's native SIMD register for 2-wide masks, but the interface abstracts away the details.
         * @details The actual layout and representation of the data in storage_ is platform-specific and may use SIMD intrinsics for efficient access. The interface provides methods to load/store from memory and perform operations without exposing the internal representation.
         */
        alignas(16) std::byte storage_[16];
    };

    static_assert(sizeof(mask64x2) == 16, "catalyst::math::mask64x2 must be 16 bytes");
    static_assert(alignof(mask64x2) == 16, "catalyst::math::mask64x2 must be 16-byte aligned");

    /**
     * @class i64x2
     * @brief A 128-bit wide SIMD vector type representing 2 lanes of 64-bit signed integer values. Provides basic arithmetic operations, bitwise operations, and utility functions for working with 2-wide vectors of 64-bit integers. The actual storage is opaque and platform-specific, but the interface allows for constructing vectors, performing element-wise operations, and loading/storing from memory.
     */
    class alignas(16) i64x2
    {
    public:
        /**
         * @fn i64x2
         * @brief Default constructor initializes the vector to all zeros.
         */
        i64x2() noexcept;

        /**
         * @fn zero
         * @brief Returns a vector with all lanes set to zero.
         * @return An i64x2 vector where all lanes are zero.
         */
        [[nodiscard]] static i64x2 zero() noexcept;
        /**
         * @fn set
         * @brief Creates a vector from 2 individual int64_t values for each lane.
         * @param x0 Value for lane 0 (least significant lane).
         * @param x1 Value for lane 1 (most significant lane).
         * @return An i64x2 vector constructed from the given values.
         */
        [[nodiscard]] static i64x2 set(std::int64_t x0, std::int64_t x1) noexcept;
        /**
         * @fn splat
         * @brief Creates a vector with all lanes set to the same int64_t value.
         * @param x The int64_t value to replicate across both lanes.
         * @return An i64x2 vector with all lanes set to x.
         */
        [[nodiscard]] static i64x2 splat(std::int64_t x) noexcept;

        /**
         * @fn load_aligned
         * @brief Loads a vector from memory with the assumption that the pointer is 16-byte aligned. The memory should contain 2 contiguous int64_t values that will be loaded into the vector lanes.
         * @param ptr Pointer to an array of 2 int64_t values to load. Must be 16-byte aligned.
         * @return An i64x2 vector loaded from the given memory.
         */
        [[nodiscard]] static i64x2 load_aligned(const std::int64_t *ptr) noexcept;
        /**
         * @fn load_unaligned
         * @brief Loads a vector from memory without any alignment requirements. The memory should contain 2 contiguous int64_t values that will be loaded into the vector lanes.
         * @param ptr Pointer to an array of 2 int64_t values to load. Does not need to be aligned.
         * @return An i64x2 vector loaded from the given memory.
         */
        [[nodiscard]] static i64x2 load_unaligned(const std::int64_t *ptr) noexcept;

        /**
         * @fn store_aligned
         * @brief Stores the vector to memory with the assumption that the pointer is 16-byte aligned. The vector's 2 lanes will be stored as contiguous int64_t values in memory.
         * @param ptr Pointer to an array of 2 int64_t values where the vector will be stored. Must be 16-byte aligned.
         */
        void store_aligned(std::int64_t *ptr) const noexcept;
        /**
         * @fn store_unaligned
         * @brief Stores the vector to memory without any alignment requirements. The vector's 2 lanes will be stored as contiguous int64_t values in memory.
         * @param ptr Pointer to an array of 2 int64_t values where the vector will be stored. Does not need to be aligned.
         */
        void store_unaligned(std::int64_t *ptr) const noexcept;

        /**
         * @fn operator+
         * @brief Element-wise addition of two vectors. Each lane of the result is the sum of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new i64x2 vector that is the element-wise sum of a and b.
         */
        friend i64x2 operator+(i64x2 a, i64x2 b) noexcept;
        /**
         * @fn operator-
         * @brief Element-wise subtraction of two vectors. Each lane of the result is the difference of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new i64x2 vector that is the element-wise difference of a and b.
         */
        friend i64x2 operator-(i64x2 a, i64x2 b) noexcept;

        /**
         * @fn operator&
         * @brief Bitwise AND of two vectors. Each lane of the result is the bitwise AND of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new i64x2 vector that is the bitwise AND of a and b.
         */
        friend i64x2 operator&(i64x2 a, i64x2 b) noexcept;
        /**
         * @fn operator|
         * @brief Bitwise OR of two vectors. Each lane of the result is the bitwise OR of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new i64x2 vector that is the bitwise OR of a and b.
         */
        friend i64x2 operator|(i64x2 a, i64x2 b) noexcept;
        /**
         * @fn operator^
         * @brief Bitwise XOR of two vectors. Each lane of the result is the bitwise XOR of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new i64x2 vector that is the bitwise XOR of a and b.
         */
        friend i64x2 operator^(i64x2 a, i64x2 b) noexcept;
        /**
         * @fn operator~
         * @brief Bitwise NOT of a vector. Each lane of the result is the bitwise NOT of the corresponding lane of the input vector.
         * @param a Vector operand.
         * @return A new i64x2 vector that is the bitwise NOT of a.
         */
        friend i64x2 operator~(i64x2 a) noexcept;

        /**
         * @fn operator+=
         * @brief Element-wise addition assignment. Updates this vector to be the element-wise sum of itself and another vector.
         * @param other Vector to add to this vector.
         * @return Reference to this vector after the operation.
         */
        i64x2 &operator+=(i64x2 other) noexcept;
        /**
         * @fn operator-=
         * @brief Element-wise subtraction assignment. Updates this vector to be the element-wise difference of itself and another vector.
         * @param other Vector to subtract from this vector.
         * @return Reference to this vector after the operation.
         */
        i64x2 &operator-=(i64x2 other) noexcept;
        /**
         * @fn operator&=
         * @brief Bitwise AND assignment. Updates this vector to be the bitwise AND of itself and another vector.
         * @param other Vector to AND with this vector.
         * @return Reference to this vector after the operation.
         */
        i64x2 &operator&=(i64x2 other) noexcept;
        /**
         * @fn operator|=
         * @brief Bitwise OR assignment. Updates this vector to be the bitwise OR of itself and another vector.
         * @param other Vector to OR with this vector.
         * @return Reference to this vector after the operation.
         */
        i64x2 &operator|=(i64x2 other) noexcept;
        /**
         * @fn operator^=
         * @brief Bitwise XOR assignment. Updates this vector to be the bitwise XOR of itself and another vector.
         * @param other Vector to XOR with this vector.
         * @return Reference to this vector after the operation.
         */
        i64x2 &operator^=(i64x2 other) noexcept;

    private:
        friend struct detail::i64x2_access;

        /**
         * @brief Opaque storage for 2x 64-bit signed integer values. The implementation interprets this memory as the platform's native SIMD register for 2-wide integers (e.g. __m128i on x86 SSE), but the interface abstracts away the details.
         * @details The actual layout and representation of the data in storage_ is platform-specific and may use SIMD intrinsics for efficient access. The interface provides methods to load/store from memory and perform operations without exposing the internal representation.
         */
        alignas(16) std::byte storage_[16];
    };

    static_assert(sizeof(i64x2) == 16, "catalyst::math::i64x2 must be 16 bytes");
    static_assert(alignof(i64x2) == 16, "catalyst::math::i64x2 must be 16-byte aligned");

    /**
     * @class u64x2
     * @brief A 128-bit wide SIMD vector type representing 2 lanes of 64-bit unsigned integer values. Provides basic arithmetic operations, bitwise operations, and utility functions for working with 2-wide vectors of 64-bit unsigned integers. The actual storage is opaque and platform-specific, but the interface allows for constructing vectors, performing element-wise operations, and loading/storing from memory.
     */
    class alignas(16) u64x2
    {
    public:
        /**
         * @fn u64x2
         * @brief Default constructor initializes the vector to all zeros.
         */
        u64x2() noexcept;

        /**
         * @fn zero
         * @brief Returns a vector with all lanes set to zero.
         * @return A u64x2 vector where all lanes are zero.
         */
        [[nodiscard]] static u64x2 zero() noexcept;
        /**
         * @fn set
         * @brief Creates a vector from 2 individual uint64_t values for each lane.
         * @param x0 Value for lane 0 (least significant lane).
         * @param x1 Value for lane 1 (most significant lane).
         * @return A u64x2 vector constructed from the given values.
         */
        [[nodiscard]] static u64x2 set(std::uint64_t x0, std::uint64_t x1) noexcept;
        /**
         * @fn splat
         * @brief Creates a vector with all lanes set to the same uint64_t value.
         * @param x The uint64_t value to replicate across both lanes.
         * @return A u64x2 vector with all lanes set to x.
         */
        [[nodiscard]] static u64x2 splat(std::uint64_t x) noexcept;

        /**
         * @fn load_aligned
         * @brief Loads a vector from memory with the assumption that the pointer is 16-byte aligned. The memory should contain 2 contiguous uint64_t values that will be loaded into the vector lanes.
         * @param ptr Pointer to an array of 2 uint64_t values to load. Must be 16-byte aligned.
         * @return A u64x2 vector loaded from the given memory.
         */
        [[nodiscard]] static u64x2 load_aligned(const std::uint64_t *ptr) noexcept;
        /**
         * @fn load_unaligned
         * @brief Loads a vector from memory without any alignment requirements. The memory should contain 2 contiguous uint64_t values that will be loaded into the vector lanes.
         * @param ptr Pointer to an array of 2 uint64_t values to load. Does not need to be aligned.
         * @return A u64x2 vector loaded from the given memory.
         */
        [[nodiscard]] static u64x2 load_unaligned(const std::uint64_t *ptr) noexcept;

        /**
         * @fn store_aligned
         * @brief Stores the vector to memory with the assumption that the pointer is 16-byte aligned. The vector's 2 lanes will be stored as contiguous uint64_t values in memory.
         * @param ptr Pointer to an array of 2 uint64_t values where the vector will be stored. Must be 16-byte aligned.
         */
        void store_aligned(std::uint64_t *ptr) const noexcept;
        /**
         * @fn store_unaligned
         * @brief Stores the vector to memory without any alignment requirements. The vector's 2 lanes will be stored as contiguous uint64_t values in memory.
         * @param ptr Pointer to an array of 2 uint64_t values where the vector will be stored. Does not need to be aligned.
         */
        void store_unaligned(std::uint64_t *ptr) const noexcept;

        /**
         * @fn operator+
         * @brief Element-wise addition of two vectors. Each lane of the result is the sum of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new u64x2 vector that is the element-wise sum of a and b.
         */
        friend u64x2 operator+(u64x2 a, u64x2 b) noexcept;
        /**
         * @fn operator-
         * @brief Element-wise subtraction of two vectors. Each lane of the result is the difference of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new u64x2 vector that is the element-wise difference of a and b.
         */
        friend u64x2 operator-(u64x2 a, u64x2 b) noexcept;

        /**
         * @fn operator&
         * @brief Bitwise AND of two vectors. Each lane of the result is the bitwise AND of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new u64x2 vector that is the bitwise AND of a and b.
         */
        friend u64x2 operator&(u64x2 a, u64x2 b) noexcept;
        /**
         * @fn operator|
         * @brief Bitwise OR of two vectors. Each lane of the result is the bitwise OR of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new u64x2 vector that is the bitwise OR of a and b.
         */
        friend u64x2 operator|(u64x2 a, u64x2 b) noexcept;
        /**
         * @fn operator^
         * @brief Bitwise XOR of two vectors. Each lane of the result is the bitwise XOR of the corresponding lanes of the input vectors.
         * @param a First vector operand.
         * @param b Second vector operand.
         * @return A new u64x2 vector that is the bitwise XOR of a and b.
         */
        friend u64x2 operator^(u64x2 a, u64x2 b) noexcept;
        /**
         * @fn operator~
         * @brief Bitwise NOT of a vector. Each lane of the result is the bitwise NOT of the corresponding lane of the input vector.
         * @param a Vector operand.
         * @return A new u64x2 vector that is the bitwise NOT of a.
         */
        friend u64x2 operator~(u64x2 a) noexcept;

        /**
         * @fn operator+=
         * @brief Element-wise addition assignment. Updates this vector to be the element-wise sum of itself and another vector.
         * @param other Vector to add to this vector.
         * @return Reference to this vector after the operation.
         */
        u64x2 &operator+=(u64x2 other) noexcept;
        /**
         * @fn operator-=
         * @brief Element-wise subtraction assignment. Updates this vector to be the element-wise difference of itself and another vector.
         * @param other Vector to subtract from this vector.
         * @return Reference to this vector after the operation.
         */
        u64x2 &operator-=(u64x2 other) noexcept;
        /**
         * @fn operator&=
         * @brief Bitwise AND assignment. Updates this vector to be the bitwise AND of itself and another vector.
         * @param other Vector to AND with this vector.
         * @return Reference to this vector after the operation.
         */
        u64x2 &operator&=(u64x2 other) noexcept;
        /**
         * @fn operator|=
         * @brief Bitwise OR assignment. Updates this vector to be the bitwise OR of itself and another vector.
         * @param other Vector to OR with this vector.
         * @return Reference to this vector after the operation.
         */
        u64x2 &operator|=(u64x2 other) noexcept;
        /**
         * @fn operator^=
         * @brief Bitwise XOR assignment. Updates this vector to be the bitwise XOR of itself and another vector.
         * @param other Vector to XOR with this vector.
         * @return Reference to this vector after the operation.
         */
        u64x2 &operator^=(u64x2 other) noexcept;

    private:
        friend struct detail::u64x2_access;

        /**
         * @brief Opaque storage for 2x 64-bit unsigned integer values. The implementation interprets this memory as the platform's native SIMD register for 2-wide unsigned integers, but the interface abstracts away the details.
         * @details The actual layout and representation of the data in storage_ is platform-specific and may use SIMD intrinsics for efficient access. The interface provides methods to load/store from memory and perform operations without exposing the internal representation.
         */
        alignas(16) std::byte storage_[16];
    };

    static_assert(sizeof(u64x2) == 16, "catalyst::math::u64x2 must be 16 bytes");
    static_assert(alignof(u64x2) == 16, "catalyst::math::u64x2 must be 16-byte aligned");

    /** @name Comparison helpers
     *  Per-lane comparison functions return explicit mask types. Use `select` to blend by mask,
     *  and `any`/`all` to reduce a mask to a boolean.
     */
    ///@{
    /** @brief Per-lane equality compare for `f32x4`. */
    [[nodiscard]] mask32x4 cmp_eq(f32x4 a, f32x4 b) noexcept;
    /** @brief Per-lane less-than compare for `f32x4`. */
    [[nodiscard]] mask32x4 cmp_lt(f32x4 a, f32x4 b) noexcept;
    /** @brief Per-lane less-than-or-equal compare for `f32x4`. */
    [[nodiscard]] mask32x4 cmp_le(f32x4 a, f32x4 b) noexcept;
    /** @brief Per-lane greater-than compare for `f32x4`. */
    [[nodiscard]] mask32x4 cmp_gt(f32x4 a, f32x4 b) noexcept;
    /** @brief Per-lane greater-than-or-equal compare for `f32x4`. */
    [[nodiscard]] mask32x4 cmp_ge(f32x4 a, f32x4 b) noexcept;

    /** @brief Lane-wise select for `f32x4` (true selects from `a`, false from `b`). */
    [[nodiscard]] f32x4 select(mask32x4 mask, f32x4 a, f32x4 b) noexcept;

    /** @brief Returns true if any lane in the `mask32x4` is true. */
    [[nodiscard]] bool any(mask32x4 mask) noexcept;
    /** @brief Returns true if all lanes in the `mask32x4` are true. */
    [[nodiscard]] bool all(mask32x4 mask) noexcept;

    /** @brief Per-lane equality compare for `f64x2`. */
    [[nodiscard]] mask64x2 cmp_eq(f64x2 a, f64x2 b) noexcept;
    /** @brief Per-lane less-than compare for `f64x2`. */
    [[nodiscard]] mask64x2 cmp_lt(f64x2 a, f64x2 b) noexcept;
    /** @brief Per-lane less-than-or-equal compare for `f64x2`. */
    [[nodiscard]] mask64x2 cmp_le(f64x2 a, f64x2 b) noexcept;
    /** @brief Per-lane greater-than compare for `f64x2`. */
    [[nodiscard]] mask64x2 cmp_gt(f64x2 a, f64x2 b) noexcept;
    /** @brief Per-lane greater-than-or-equal compare for `f64x2`. */
    [[nodiscard]] mask64x2 cmp_ge(f64x2 a, f64x2 b) noexcept;

    /** @brief Lane-wise select for `f64x2` (true selects from `a`, false from `b`). */
    [[nodiscard]] f64x2 select(mask64x2 mask, f64x2 a, f64x2 b) noexcept;

    /** @brief Returns true if any lane in the `mask64x2` is true. */
    [[nodiscard]] bool any(mask64x2 mask) noexcept;
    /** @brief Returns true if all lanes in the `mask64x2` are true. */
    [[nodiscard]] bool all(mask64x2 mask) noexcept;
    ///@}

} // namespace catalyst::math