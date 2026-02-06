#pragma once

/**
 * @file simd.hpp
 * @brief Opaque SIMD vector/mask types and operations.
 *
 * This header exposes a small, cross-platform SIMD surface area while keeping ISA-specific types
 * (SSE/NEON/etc.) out of the public API.
 *
 * Design goals:
 * - Opaque storage: no `__m128`, `float32x4_t`, etc. in the header
 * - Cross-platform: the implementation selects the best available backend at build time
 * - Explicit alignment semantics for loads/stores
 *
 * Notes:
 * - All SIMD types in this header are 16 bytes and 16-byte aligned.
 * - `*_aligned` loads/stores require a 16-byte aligned pointer.
 * - Comparisons return explicit mask types (`mask32x4`, `mask64x2`). A mask lane is considered
 *   true when its lane bits are all-ones and false when they are all-zeros.
 */

#include <cstddef>
#include <cstdint>

namespace catalyst::math
{

/// \cond INTERNAL
// Internal helper macros used to keep the declaration surface compact.

#define CATALYST_MATH_SIMD_STATIC_ASSERTS(type_name)                                           \
    static_assert(sizeof(type_name) == 16, "catalyst::math::" #type_name " must be 16 bytes"); \
    static_assert(alignof(type_name) == 16, "catalyst::math::" #type_name " must be 16-byte aligned")

#define CATALYST_MATH_SIMD_DECLARE_MASK(type_name, access_name, bit_type, lanes)     \
    class alignas(16) type_name                                                      \
    {                                                                                \
    public:                                                                          \
        type_name() noexcept;                                                        \
                                                                                     \
        [[nodiscard]] static type_name all_false() noexcept;                         \
        [[nodiscard]] static type_name all_true() noexcept;                          \
        [[nodiscard]] static type_name from_bits(bit_type b0, bit_type b1) noexcept; \
                                                                                     \
        void store_unaligned(bit_type *ptr) const noexcept;                          \
                                                                                     \
        friend type_name operator&(type_name a, type_name b) noexcept;               \
        friend type_name operator|(type_name a, type_name b) noexcept;               \
        friend type_name operator^(type_name a, type_name b) noexcept;               \
        friend type_name operator~(type_name a) noexcept;                            \
                                                                                     \
        type_name &operator&=(type_name other) noexcept;                             \
        type_name &operator|=(type_name other) noexcept;                             \
        type_name &operator^=(type_name other) noexcept;                             \
                                                                                     \
    private:                                                                         \
        friend struct detail::access_name;                                           \
        alignas(16) std::byte storage_[16];                                          \
    };                                                                               \
                                                                                     \
    CATALYST_MATH_SIMD_STATIC_ASSERTS(type_name)

#define CATALYST_MATH_SIMD_DECLARE_MASK32(type_name, access_name)                                      \
    class alignas(16) type_name                                                                        \
    {                                                                                                  \
    public:                                                                                            \
        type_name() noexcept;                                                                          \
                                                                                                       \
        [[nodiscard]] static type_name all_false() noexcept;                                           \
        [[nodiscard]] static type_name all_true() noexcept;                                            \
        [[nodiscard]] static type_name from_bits(std::uint32_t b0, std::uint32_t b1, std::uint32_t b2, \
                                                 std::uint32_t b3) noexcept;                           \
                                                                                                       \
        void store_unaligned(std::uint32_t *ptr) const noexcept;                                       \
                                                                                                       \
        friend type_name operator&(type_name a, type_name b) noexcept;                                 \
        friend type_name operator|(type_name a, type_name b) noexcept;                                 \
        friend type_name operator^(type_name a, type_name b) noexcept;                                 \
        friend type_name operator~(type_name a) noexcept;                                              \
                                                                                                       \
        type_name &operator&=(type_name other) noexcept;                                               \
        type_name &operator|=(type_name other) noexcept;                                               \
        type_name &operator^=(type_name other) noexcept;                                               \
                                                                                                       \
    private:                                                                                           \
        friend struct detail::access_name;                                                             \
        alignas(16) std::byte storage_[16];                                                            \
    };                                                                                                 \
                                                                                                       \
    CATALYST_MATH_SIMD_STATIC_ASSERTS(type_name)

#define CATALYST_MATH_SIMD_DECLARE_INT_VEC(type_name, access_name, scalar_type, signedness_name)                     \
    class alignas(16) type_name                                                                                      \
    {                                                                                                                \
    public:                                                                                                          \
        type_name() noexcept;                                                                                        \
                                                                                                                     \
        [[nodiscard]] static type_name zero() noexcept;                                                              \
        [[nodiscard]] static type_name set(scalar_type x0, scalar_type x1, scalar_type x2, scalar_type x3) noexcept; \
        [[nodiscard]] static type_name splat(scalar_type x) noexcept;                                                \
                                                                                                                     \
        [[nodiscard]] static type_name load_aligned(const scalar_type *ptr) noexcept;                                \
        [[nodiscard]] static type_name load_unaligned(const scalar_type *ptr) noexcept;                              \
                                                                                                                     \
        void store_aligned(scalar_type *ptr) const noexcept;                                                         \
        void store_unaligned(scalar_type *ptr) const noexcept;                                                       \
                                                                                                                     \
        friend type_name operator+(type_name a, type_name b) noexcept;                                               \
        friend type_name operator-(type_name a, type_name b) noexcept;                                               \
                                                                                                                     \
        friend type_name operator&(type_name a, type_name b) noexcept;                                               \
        friend type_name operator|(type_name a, type_name b) noexcept;                                               \
        friend type_name operator^(type_name a, type_name b) noexcept;                                               \
        friend type_name operator~(type_name a) noexcept;                                                            \
                                                                                                                     \
        type_name &operator+=(type_name other) noexcept;                                                             \
        type_name &operator-=(type_name other) noexcept;                                                             \
        type_name &operator&=(type_name other) noexcept;                                                             \
        type_name &operator|=(type_name other) noexcept;                                                             \
        type_name &operator^=(type_name other) noexcept;                                                             \
                                                                                                                     \
    private:                                                                                                         \
        friend struct detail::access_name;                                                                           \
        alignas(16) std::byte storage_[16];                                                                          \
    };                                                                                                               \
                                                                                                                     \
    CATALYST_MATH_SIMD_STATIC_ASSERTS(type_name)

#define CATALYST_MATH_SIMD_DECLARE_INT_VEC2(type_name, access_name, scalar_type)        \
    class alignas(16) type_name                                                         \
    {                                                                                   \
    public:                                                                             \
        type_name() noexcept;                                                           \
                                                                                        \
        [[nodiscard]] static type_name zero() noexcept;                                 \
        [[nodiscard]] static type_name set(scalar_type x0, scalar_type x1) noexcept;    \
        [[nodiscard]] static type_name splat(scalar_type x) noexcept;                   \
                                                                                        \
        [[nodiscard]] static type_name load_aligned(const scalar_type *ptr) noexcept;   \
        [[nodiscard]] static type_name load_unaligned(const scalar_type *ptr) noexcept; \
                                                                                        \
        void store_aligned(scalar_type *ptr) const noexcept;                            \
        void store_unaligned(scalar_type *ptr) const noexcept;                          \
                                                                                        \
        friend type_name operator+(type_name a, type_name b) noexcept;                  \
        friend type_name operator-(type_name a, type_name b) noexcept;                  \
                                                                                        \
        friend type_name operator&(type_name a, type_name b) noexcept;                  \
        friend type_name operator|(type_name a, type_name b) noexcept;                  \
        friend type_name operator^(type_name a, type_name b) noexcept;                  \
        friend type_name operator~(type_name a) noexcept;                               \
                                                                                        \
        type_name &operator+=(type_name other) noexcept;                                \
        type_name &operator-=(type_name other) noexcept;                                \
        type_name &operator&=(type_name other) noexcept;                                \
        type_name &operator|=(type_name other) noexcept;                                \
        type_name &operator^=(type_name other) noexcept;                                \
                                                                                        \
    private:                                                                            \
        friend struct detail::access_name;                                              \
        alignas(16) std::byte storage_[16];                                             \
    };                                                                                  \
                                                                                        \
    CATALYST_MATH_SIMD_STATIC_ASSERTS(type_name)

#define CATALYST_MATH_SIMD_DECLARE_CMPS(vec_type, mask_type)         \
    [[nodiscard]] mask_type cmp_eq(vec_type a, vec_type b) noexcept; \
    [[nodiscard]] mask_type cmp_lt(vec_type a, vec_type b) noexcept; \
    [[nodiscard]] mask_type cmp_le(vec_type a, vec_type b) noexcept; \
    [[nodiscard]] mask_type cmp_gt(vec_type a, vec_type b) noexcept; \
    [[nodiscard]] mask_type cmp_ge(vec_type a, vec_type b) noexcept

#define CATALYST_MATH_SIMD_DECLARE_MASK_FUNCS(mask_type) \
    [[nodiscard]] bool any(mask_type mask) noexcept;     \
    [[nodiscard]] bool all(mask_type mask) noexcept

/// \endcond

    /// \cond INTERNAL
    namespace detail
    {
        struct f32x4_access;
        struct mask32x4_access;
        struct i32x4_access;
        struct u32x4_access;
        struct i64x2_access;
        struct u64x2_access;
        struct f64x2_access;
        struct mask64x2_access;
    }
    /// \endcond

    /**
     * @class mask32x4
     * @brief Mask for 4 lanes of 32-bit operations.
     *
     * A lane is "true" when its lane bits are all-ones (typically `0xFFFFFFFF`) and "false" when
     * its lane bits are all-zeros.
     */
    CATALYST_MATH_SIMD_DECLARE_MASK32(mask32x4, mask32x4_access);

    /**
     * @class f32x4
     * @brief SIMD vector of 4 single-precision floats.
     */
    class alignas(16) f32x4
    {
    public:
        f32x4() noexcept;

        [[nodiscard]] static f32x4 zero() noexcept;
        [[nodiscard]] static f32x4 set(float x0, float x1, float x2, float x3) noexcept;
        [[nodiscard]] static f32x4 splat(float x) noexcept;

        [[nodiscard]] static f32x4 load_aligned(const float *ptr) noexcept;
        [[nodiscard]] static f32x4 load_unaligned(const float *ptr) noexcept;

        void store_aligned(float *ptr) const noexcept;
        void store_unaligned(float *ptr) const noexcept;

        friend f32x4 operator+(f32x4 a, f32x4 b) noexcept;
        friend f32x4 operator-(f32x4 a, f32x4 b) noexcept;
        friend f32x4 operator*(f32x4 a, f32x4 b) noexcept;
        friend f32x4 operator/(f32x4 a, f32x4 b) noexcept;

        f32x4 &operator+=(f32x4 other) noexcept;
        f32x4 &operator-=(f32x4 other) noexcept;
        f32x4 &operator*=(f32x4 other) noexcept;
        f32x4 &operator/=(f32x4 other) noexcept;

        [[nodiscard]] f32x4 abs() const noexcept;

        // Horizontal min/max: returns the min/max component splatted to all lanes.
        [[nodiscard]] f32x4 min() const noexcept;
        [[nodiscard]] f32x4 max() const noexcept;

        // Element-wise min/max.
        [[nodiscard]] f32x4 min(f32x4 other) const noexcept;
        [[nodiscard]] f32x4 max(f32x4 other) const noexcept;

        // Lane mask based on each lane's sign-bit.
        [[nodiscard]] mask32x4 mask() const noexcept;

        [[nodiscard]] f32x4 sqrt() const noexcept;
        [[nodiscard]] f32x4 rsqrt() const noexcept;
        [[nodiscard]] f32x4 reciprocal() const noexcept;

    private:
        friend struct detail::f32x4_access;

        // Opaque storage for 4x f32 values.
        // The implementation interprets this memory as the platform's native SIMD register.
        alignas(16) std::byte storage_[16];
    };

    CATALYST_MATH_SIMD_STATIC_ASSERTS(f32x4);

    
    /**
     * @class i32x4
     * @brief SIMD vector of 4 signed 32-bit integers.
     */
    CATALYST_MATH_SIMD_DECLARE_INT_VEC(i32x4, i32x4_access, std::int32_t, i32);

    
    /**
     * @class u32x4
     * @brief SIMD vector of 4 unsigned 32-bit integers.
     */
    CATALYST_MATH_SIMD_DECLARE_INT_VEC(u32x4, u32x4_access, std::uint32_t, u32);

    /**
     * @class f64x2
     * @brief SIMD vector of 2 double-precision floats.
     */

    class alignas(16) f64x2
    {
    public:
        f64x2() noexcept;

        [[nodiscard]] static f64x2 zero() noexcept;
        [[nodiscard]] static f64x2 set(double x0, double x1) noexcept;
        [[nodiscard]] static f64x2 splat(double x) noexcept;

        [[nodiscard]] static f64x2 load_aligned(const double *ptr) noexcept;
        [[nodiscard]] static f64x2 load_unaligned(const double *ptr) noexcept;

        void store_aligned(double *ptr) const noexcept;
        void store_unaligned(double *ptr) const noexcept;

        friend f64x2 operator+(f64x2 a, f64x2 b) noexcept;
        friend f64x2 operator-(f64x2 a, f64x2 b) noexcept;
        friend f64x2 operator*(f64x2 a, f64x2 b) noexcept;
        friend f64x2 operator/(f64x2 a, f64x2 b) noexcept;

        f64x2 &operator+=(f64x2 other) noexcept;
        f64x2 &operator-=(f64x2 other) noexcept;
        f64x2 &operator*=(f64x2 other) noexcept;
        f64x2 &operator/=(f64x2 other) noexcept;

        [[nodiscard]] f64x2 abs() const noexcept;
        [[nodiscard]] f64x2 min(f64x2 other) const noexcept;
        [[nodiscard]] f64x2 max(f64x2 other) const noexcept;
        [[nodiscard]] f64x2 sqrt() const noexcept;
        [[nodiscard]] f64x2 reciprocal() const noexcept;
        [[nodiscard]] f64x2 rsqrt() const noexcept;

    private:
        friend struct detail::f64x2_access;
        alignas(16) std::byte storage_[16];
    };

    CATALYST_MATH_SIMD_STATIC_ASSERTS(f64x2);

    /**
     * @class mask64x2
     * @brief Mask for 2 lanes of 64-bit operations.
     */
    CATALYST_MATH_SIMD_DECLARE_MASK(mask64x2, mask64x2_access, std::uint64_t, 2);

    /**
     * @class i64x2
     * @brief SIMD vector of 2 signed 64-bit integers.
     */
    CATALYST_MATH_SIMD_DECLARE_INT_VEC2(i64x2, i64x2_access, std::int64_t);

    /**
     * @class u64x2
     * @brief SIMD vector of 2 unsigned 64-bit integers.
     */
    CATALYST_MATH_SIMD_DECLARE_INT_VEC2(u64x2, u64x2_access, std::uint64_t);

    /** @name f32x4 comparisons and masks
     *  Comparisons return a mask: all bits set per-lane if true, else 0.
     */
    ///@{
    /** @fn mask32x4 cmp_eq(f32x4 a, f32x4 b) noexcept
     *  @brief Per-lane equality compare.
     */
    [[nodiscard]] mask32x4 cmp_eq(f32x4 a, f32x4 b) noexcept;
    /** @fn mask32x4 cmp_lt(f32x4 a, f32x4 b) noexcept
     *  @brief Per-lane less-than compare.
     */
    [[nodiscard]] mask32x4 cmp_lt(f32x4 a, f32x4 b) noexcept;
    /** @fn mask32x4 cmp_le(f32x4 a, f32x4 b) noexcept
     *  @brief Per-lane less-than-or-equal compare.
     */
    [[nodiscard]] mask32x4 cmp_le(f32x4 a, f32x4 b) noexcept;
    /** @fn mask32x4 cmp_gt(f32x4 a, f32x4 b) noexcept
     *  @brief Per-lane greater-than compare.
     */
    [[nodiscard]] mask32x4 cmp_gt(f32x4 a, f32x4 b) noexcept;
    /** @fn mask32x4 cmp_ge(f32x4 a, f32x4 b) noexcept
     *  @brief Per-lane greater-than-or-equal compare.
     */
    [[nodiscard]] mask32x4 cmp_ge(f32x4 a, f32x4 b) noexcept;

    /**
     * @brief Lane-wise select.
     * @param mask Per-lane mask (true selects from @p a, false selects from @p b).
     */
    [[nodiscard]] f32x4 select(mask32x4 mask, f32x4 a, f32x4 b) noexcept;

    /**
        * @brief Returns true if any lane of the mask is true.
     */
        [[nodiscard]] bool any(mask32x4 mask) noexcept;

        /**
        * @brief Returns true if all lanes of the mask are true.
        */
        [[nodiscard]] bool all(mask32x4 mask) noexcept;

        ///@}

        /** @name f64x2 comparisons and masks
        *  Comparisons return a mask: all bits set per-lane if true, else 0.
        */
        ///@{
    /** @fn mask64x2 cmp_eq(f64x2 a, f64x2 b) noexcept
     *  @brief Per-lane equality compare.
     */
    [[nodiscard]] mask64x2 cmp_eq(f64x2 a, f64x2 b) noexcept;
    /** @fn mask64x2 cmp_lt(f64x2 a, f64x2 b) noexcept
     *  @brief Per-lane less-than compare.
     */
    [[nodiscard]] mask64x2 cmp_lt(f64x2 a, f64x2 b) noexcept;
    /** @fn mask64x2 cmp_le(f64x2 a, f64x2 b) noexcept
     *  @brief Per-lane less-than-or-equal compare.
     */
    [[nodiscard]] mask64x2 cmp_le(f64x2 a, f64x2 b) noexcept;
    /** @fn mask64x2 cmp_gt(f64x2 a, f64x2 b) noexcept
     *  @brief Per-lane greater-than compare.
     */
    [[nodiscard]] mask64x2 cmp_gt(f64x2 a, f64x2 b) noexcept;
    /** @fn mask64x2 cmp_ge(f64x2 a, f64x2 b) noexcept
     *  @brief Per-lane greater-than-or-equal compare.
     */
    [[nodiscard]] mask64x2 cmp_ge(f64x2 a, f64x2 b) noexcept;

    /**
     * @brief Lane-wise select.
     * @param mask Per-lane mask (true selects from @p a, false selects from @p b).
     */
    [[nodiscard]] f64x2 select(mask64x2 mask, f64x2 a, f64x2 b) noexcept;

    /**
     * @brief Returns true if any lane of the mask is true.
     */
    [[nodiscard]] bool any(mask64x2 mask) noexcept;

    /**
     * @brief Returns true if all lanes of the mask are true.
     */
    [[nodiscard]] bool all(mask64x2 mask) noexcept;

    ///@}

} // namespace catalyst::math

#undef CATALYST_MATH_SIMD_DECLARE_MASK_FUNCS
#undef CATALYST_MATH_SIMD_DECLARE_CMPS
#undef CATALYST_MATH_SIMD_DECLARE_INT_VEC2
#undef CATALYST_MATH_SIMD_DECLARE_INT_VEC
#undef CATALYST_MATH_SIMD_DECLARE_MASK32
#undef CATALYST_MATH_SIMD_DECLARE_MASK
#undef CATALYST_MATH_SIMD_STATIC_ASSERTS