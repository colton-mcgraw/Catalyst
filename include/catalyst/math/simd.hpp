#pragma once

// SIMD math API.
//
// Design goals:
// - Opaque storage: header does not expose ISA-specific types (__m128/NEON/etc.)
// - Cross-platform: implementation selects best available backend at build time
// - Explicit alignment semantics for loads/stores

#include <cstddef>
#include <cstdint>

namespace catalyst::math
{

    namespace detail
    {
        struct f32x4_access;
    }

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

        // Lane mask (0xFFFFFFFF for negative lanes, else 0).
        [[nodiscard]] f32x4 mask() const noexcept;

        [[nodiscard]] f32x4 sqrt() const noexcept;
        [[nodiscard]] f32x4 rsqrt() const noexcept;
        [[nodiscard]] f32x4 reciprocal() const noexcept;

    private:
        friend struct detail::f32x4_access;

        // Opaque storage for 4x f32 values.
        // The implementation interprets this memory as the platform's native SIMD register.
        alignas(16) std::byte storage_[16];
    };

    static_assert(sizeof(f32x4) == 16, "catalyst::math::f32x4 must be 16 bytes");
    static_assert(alignof(f32x4) == 16, "catalyst::math::f32x4 must be 16-byte aligned");

} // namespace catalyst::math