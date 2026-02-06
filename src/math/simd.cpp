#include <catalyst/math/simd.hpp>

#include <cmath>
#include <cstring>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#define CATALYST_MATH_HAS_SSE2 1
#else
#define CATALYST_MATH_HAS_SSE2 0
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define CATALYST_MATH_HAS_NEON 1
#else
#define CATALYST_MATH_HAS_NEON 0
#endif

namespace catalyst::math
{

    namespace detail
    {
        struct f32x4_access
        {
            static std::byte *data(f32x4 &v) noexcept { return v.storage_; }
            static const std::byte *data(const f32x4 &v) noexcept { return v.storage_; }
        };

                struct mask32x4_access
                {
                        static std::byte *data(mask32x4 &v) noexcept { return v.storage_; }
                        static const std::byte *data(const mask32x4 &v) noexcept { return v.storage_; }
                };

        struct i32x4_access
        {
            static std::byte *data(i32x4 &v) noexcept { return v.storage_; }
            static const std::byte *data(const i32x4 &v) noexcept { return v.storage_; }
        };

        struct u32x4_access
        {
            static std::byte *data(u32x4 &v) noexcept { return v.storage_; }
            static const std::byte *data(const u32x4 &v) noexcept { return v.storage_; }
        };

        struct f64x2_access
        {
            static std::byte *data(f64x2 &v) noexcept { return v.storage_; }
            static const std::byte *data(const f64x2 &v) noexcept { return v.storage_; }
        };

                struct mask64x2_access
                {
                        static std::byte *data(mask64x2 &v) noexcept { return v.storage_; }
                        static const std::byte *data(const mask64x2 &v) noexcept { return v.storage_; }
                };

        struct i64x2_access
        {
            static std::byte *data(i64x2 &v) noexcept { return v.storage_; }
            static const std::byte *data(const i64x2 &v) noexcept { return v.storage_; }
        };

        struct u64x2_access
        {
            static std::byte *data(u64x2 &v) noexcept { return v.storage_; }
            static const std::byte *data(const u64x2 &v) noexcept { return v.storage_; }
        };
    }

    namespace
    {

#if CATALYST_MATH_HAS_SSE2
        inline __m128 to_m128(const f32x4 &v) noexcept
        {
            __m128 r;
            std::memcpy(&r, detail::f32x4_access::data(v), sizeof(r));
            return r;
        }

        inline f32x4 from_m128(__m128 r) noexcept
        {
            f32x4 v;
            std::memcpy(detail::f32x4_access::data(v), &r, 16);
            return v;
        }

                inline __m128 to_m128_mask(const mask32x4 &v) noexcept
                {
                        __m128 r;
                        std::memcpy(&r, detail::mask32x4_access::data(v), sizeof(r));
                        return r;
                }

                inline mask32x4 from_m128_mask(__m128 r) noexcept
                {
                        mask32x4 v;
                        std::memcpy(detail::mask32x4_access::data(v), &r, 16);
                        return v;
                }
#endif

#if CATALYST_MATH_HAS_SSE2
        inline __m128i to_m128i(const i32x4 &v) noexcept
        {
            __m128i r;
            std::memcpy(&r, detail::i32x4_access::data(v), sizeof(r));
            return r;
        }

        inline __m128i to_m128i(const u32x4 &v) noexcept
        {
            __m128i r;
            std::memcpy(&r, detail::u32x4_access::data(v), sizeof(r));
            return r;
        }

        inline i32x4 from_m128i_i32(__m128i r) noexcept
        {
            i32x4 v;
            std::memcpy(detail::i32x4_access::data(v), &r, 16);
            return v;
        }

        inline u32x4 from_m128i_u32(__m128i r) noexcept
        {
            u32x4 v;
            std::memcpy(detail::u32x4_access::data(v), &r, 16);
            return v;
        }

        inline __m128i to_m128i(const i64x2 &v) noexcept
        {
            __m128i r;
            std::memcpy(&r, detail::i64x2_access::data(v), sizeof(r));
            return r;
        }

        inline __m128i to_m128i(const u64x2 &v) noexcept
        {
            __m128i r;
            std::memcpy(&r, detail::u64x2_access::data(v), sizeof(r));
            return r;
        }

        inline i64x2 from_m128i_i64(__m128i r) noexcept
        {
            i64x2 v;
            std::memcpy(detail::i64x2_access::data(v), &r, 16);
            return v;
        }

        inline u64x2 from_m128i_u64(__m128i r) noexcept
        {
            u64x2 v;
            std::memcpy(detail::u64x2_access::data(v), &r, 16);
            return v;
        }

        inline __m128d to_m128d(const f64x2 &v) noexcept
        {
            __m128d r;
            std::memcpy(&r, detail::f64x2_access::data(v), sizeof(r));
            return r;
        }

        inline f64x2 from_m128d(__m128d r) noexcept
        {
            f64x2 v;
            std::memcpy(detail::f64x2_access::data(v), &r, 16);
            return v;
        }

                inline __m128d to_m128d_mask(const mask64x2 &v) noexcept
                {
                        __m128d r;
                        std::memcpy(&r, detail::mask64x2_access::data(v), sizeof(r));
                        return r;
                }

                inline mask64x2 from_m128d_mask(__m128d r) noexcept
                {
                        mask64x2 v;
                        std::memcpy(detail::mask64x2_access::data(v), &r, 16);
                        return v;
                }
#endif

#if CATALYST_MATH_HAS_NEON
        inline float32x4_t to_neon(const f32x4 &v) noexcept
        {
            float32x4_t r;
            std::memcpy(&r, detail::f32x4_access::data(v), sizeof(r));
            return r;
        }

        inline f32x4 from_neon(float32x4_t r) noexcept
        {
            f32x4 v;
            std::memcpy(detail::f32x4_access::data(v), &r, 16);
            return v;
        }

                inline uint32x4_t to_neon_mask32(const mask32x4 &v) noexcept
                {
                        uint32x4_t r;
                        std::memcpy(&r, detail::mask32x4_access::data(v), sizeof(r));
                        return r;
                }

                inline mask32x4 from_neon_mask32(uint32x4_t r) noexcept
                {
                        mask32x4 v;
                        std::memcpy(detail::mask32x4_access::data(v), &r, 16);
                        return v;
                }
#endif

#if CATALYST_MATH_HAS_NEON
        inline int32x4_t to_neon_i32(const i32x4 &v) noexcept
        {
            int32x4_t r;
            std::memcpy(&r, detail::i32x4_access::data(v), sizeof(r));
            return r;
        }

        inline uint32x4_t to_neon_u32(const u32x4 &v) noexcept
        {
            uint32x4_t r;
            std::memcpy(&r, detail::u32x4_access::data(v), sizeof(r));
            return r;
        }

        inline i32x4 from_neon_i32(int32x4_t r) noexcept
        {
            i32x4 v;
            std::memcpy(detail::i32x4_access::data(v), &r, 16);
            return v;
        }

        inline u32x4 from_neon_u32(uint32x4_t r) noexcept
        {
            u32x4 v;
            std::memcpy(detail::u32x4_access::data(v), &r, 16);
            return v;
        }

#if defined(__aarch64__)
        inline float64x2_t to_neon_f64(const f64x2 &v) noexcept
        {
            float64x2_t r;
            std::memcpy(&r, detail::f64x2_access::data(v), sizeof(r));
            return r;
        }

        inline f64x2 from_neon_f64(float64x2_t r) noexcept
        {
            f64x2 v;
            std::memcpy(detail::f64x2_access::data(v), &r, 16);
            return v;
        }

                inline uint64x2_t to_neon_mask64(const mask64x2 &v) noexcept
                {
                        uint64x2_t r;
                        std::memcpy(&r, detail::mask64x2_access::data(v), sizeof(r));
                        return r;
                }

                inline mask64x2 from_neon_mask64(uint64x2_t r) noexcept
                {
                        mask64x2 v;
                        std::memcpy(detail::mask64x2_access::data(v), &r, 16);
                        return v;
                }

        inline int64x2_t to_neon_i64(const i64x2 &v) noexcept
        {
            int64x2_t r;
            std::memcpy(&r, detail::i64x2_access::data(v), sizeof(r));
            return r;
        }

        inline uint64x2_t to_neon_u64(const u64x2 &v) noexcept
        {
            uint64x2_t r;
            std::memcpy(&r, detail::u64x2_access::data(v), sizeof(r));
            return r;
        }

        inline i64x2 from_neon_i64(int64x2_t r) noexcept
        {
            i64x2 v;
            std::memcpy(detail::i64x2_access::data(v), &r, 16);
            return v;
        }

        inline u64x2 from_neon_u64(uint64x2_t r) noexcept
        {
            u64x2 v;
            std::memcpy(detail::u64x2_access::data(v), &r, 16);
            return v;
        }
#endif

#endif

        inline void store_scalar(float *out4, const f32x4 &v) noexcept
        {
            v.store_unaligned(out4);
        }

        inline f32x4 load_scalar(const float *in4) noexcept
        {
            return f32x4::load_unaligned(in4);
        }

    } // namespace

    f32x4::f32x4() noexcept
    {
        // Default initialize to zero (safe, deterministic).
        std::memset(detail::f32x4_access::data(*this), 0, 16);
    }

    f32x4 f32x4::zero() noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128(_mm_setzero_ps());
#elif CATALYST_MATH_HAS_NEON
        return from_neon(vdupq_n_f32(0.0f));
#else
        return f32x4::splat(0.0f);
#endif
    }

    f32x4 f32x4::set(float x0, float x1, float x2, float x3) noexcept
    {
        const float tmp[4] = {x0, x1, x2, x3};
        return load_unaligned(tmp);
    }

    f32x4 f32x4::splat(float x) noexcept
    {
        return set(x, x, x, x);
    }

    f32x4 f32x4::load_aligned(const float *ptr) noexcept
    {
        f32x4 v;
#if CATALYST_MATH_HAS_SSE2
        const __m128 r = _mm_load_ps(ptr);
        std::memcpy(detail::f32x4_access::data(v), &r, 16);
#elif CATALYST_MATH_HAS_NEON
        const float32x4_t r = vld1q_f32(ptr);
        std::memcpy(detail::f32x4_access::data(v), &r, 16);
#else
        std::memcpy(detail::f32x4_access::data(v), ptr, 16);
#endif
        return v;
    }

    f32x4 f32x4::load_unaligned(const float *ptr) noexcept
    {
        f32x4 v;
#if CATALYST_MATH_HAS_SSE2
        const __m128 r = _mm_loadu_ps(ptr);
        std::memcpy(detail::f32x4_access::data(v), &r, 16);
#elif CATALYST_MATH_HAS_NEON
        const float32x4_t r = vld1q_f32(ptr);
        std::memcpy(detail::f32x4_access::data(v), &r, 16);
#else
        std::memcpy(detail::f32x4_access::data(v), ptr, 16);
#endif
        return v;
    }

    void f32x4::store_aligned(float *ptr) const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        __m128 r;
        std::memcpy(&r, detail::f32x4_access::data(*this), sizeof(r));
        _mm_store_ps(ptr, r);
#elif CATALYST_MATH_HAS_NEON
        float32x4_t r;
        std::memcpy(&r, detail::f32x4_access::data(*this), sizeof(r));
        vst1q_f32(ptr, r);
#else
        std::memcpy(ptr, detail::f32x4_access::data(*this), 16);
#endif
    }

    void f32x4::store_unaligned(float *ptr) const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        __m128 r;
        std::memcpy(&r, detail::f32x4_access::data(*this), sizeof(r));
        _mm_storeu_ps(ptr, r);
#elif CATALYST_MATH_HAS_NEON
        float32x4_t r;
        std::memcpy(&r, detail::f32x4_access::data(*this), sizeof(r));
        vst1q_f32(ptr, r);
#else
        std::memcpy(ptr, detail::f32x4_access::data(*this), 16);
#endif
    }

    f32x4 operator+(f32x4 a, f32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        __m128 ra, rb;
        std::memcpy(&ra, detail::f32x4_access::data(a), sizeof(ra));
        std::memcpy(&rb, detail::f32x4_access::data(b), sizeof(rb));
        const __m128 r = _mm_add_ps(ra, rb);
        std::memcpy(detail::f32x4_access::data(a), &r, 16);
        return a;
#elif CATALYST_MATH_HAS_NEON
        float32x4_t ra, rb;
        std::memcpy(&ra, detail::f32x4_access::data(a), sizeof(ra));
        std::memcpy(&rb, detail::f32x4_access::data(b), sizeof(rb));
        const float32x4_t r = vaddq_f32(ra, rb);
        std::memcpy(detail::f32x4_access::data(a), &r, 16);
        return a;
#else
        float fa[4], fb[4];
        store_scalar(fa, a);
        store_scalar(fb, b);
        const float out[4] = {fa[0] + fb[0], fa[1] + fb[1], fa[2] + fb[2], fa[3] + fb[3]};
        return load_scalar(out);
#endif
    }

    f32x4 operator-(f32x4 a, f32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        __m128 ra, rb;
        std::memcpy(&ra, detail::f32x4_access::data(a), sizeof(ra));
        std::memcpy(&rb, detail::f32x4_access::data(b), sizeof(rb));
        const __m128 r = _mm_sub_ps(ra, rb);
        std::memcpy(detail::f32x4_access::data(a), &r, 16);
        return a;
#elif CATALYST_MATH_HAS_NEON
        float32x4_t ra, rb;
        std::memcpy(&ra, detail::f32x4_access::data(a), sizeof(ra));
        std::memcpy(&rb, detail::f32x4_access::data(b), sizeof(rb));
        const float32x4_t r = vsubq_f32(ra, rb);
        std::memcpy(detail::f32x4_access::data(a), &r, 16);
        return a;
#else
        float fa[4], fb[4];
        store_scalar(fa, a);
        store_scalar(fb, b);
        const float out[4] = {fa[0] - fb[0], fa[1] - fb[1], fa[2] - fb[2], fa[3] - fb[3]};
        return load_scalar(out);
#endif
    }

    f32x4 operator*(f32x4 a, f32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        __m128 ra, rb;
        std::memcpy(&ra, detail::f32x4_access::data(a), sizeof(ra));
        std::memcpy(&rb, detail::f32x4_access::data(b), sizeof(rb));
        const __m128 r = _mm_mul_ps(ra, rb);
        std::memcpy(detail::f32x4_access::data(a), &r, 16);
        return a;
#elif CATALYST_MATH_HAS_NEON
        float32x4_t ra, rb;
        std::memcpy(&ra, detail::f32x4_access::data(a), sizeof(ra));
        std::memcpy(&rb, detail::f32x4_access::data(b), sizeof(rb));
        const float32x4_t r = vmulq_f32(ra, rb);
        std::memcpy(detail::f32x4_access::data(a), &r, 16);
        return a;
#else
        float fa[4], fb[4];
        store_scalar(fa, a);
        store_scalar(fb, b);
        const float out[4] = {fa[0] * fb[0], fa[1] * fb[1], fa[2] * fb[2], fa[3] * fb[3]};
        return load_scalar(out);
#endif
    }

    f32x4 operator/(f32x4 a, f32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        __m128 ra, rb;
        std::memcpy(&ra, detail::f32x4_access::data(a), sizeof(ra));
        std::memcpy(&rb, detail::f32x4_access::data(b), sizeof(rb));
        const __m128 r = _mm_div_ps(ra, rb);
        std::memcpy(detail::f32x4_access::data(a), &r, 16);
        return a;
#elif CATALYST_MATH_HAS_NEON
        // NEON division is not a single instruction on many targets; start with scalar correctness.
        float fa[4], fb[4];
        store_scalar(fa, a);
        store_scalar(fb, b);
        const float out[4] = {fa[0] / fb[0], fa[1] / fb[1], fa[2] / fb[2], fa[3] / fb[3]};
        return load_scalar(out);
#else
        float fa[4], fb[4];
        store_scalar(fa, a);
        store_scalar(fb, b);
        const float out[4] = {fa[0] / fb[0], fa[1] / fb[1], fa[2] / fb[2], fa[3] / fb[3]};
        return load_scalar(out);
#endif
    }

    f32x4 &f32x4::operator+=(f32x4 other) noexcept
    {
        *this = *this + other;
        return *this;
    }

    f32x4 &f32x4::operator-=(f32x4 other) noexcept
    {
        *this = *this - other;
        return *this;
    }

    f32x4 &f32x4::operator*=(f32x4 other) noexcept
    {
        *this = *this * other;
        return *this;
    }

    f32x4 &f32x4::operator/=(f32x4 other) noexcept
    {
        *this = *this / other;
        return *this;
    }

    f32x4 f32x4::abs() const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        const __m128 x = to_m128(*this);
        const __m128 sign_mask = _mm_set1_ps(-0.0f);
        return from_m128(_mm_andnot_ps(sign_mask, x));
#elif CATALYST_MATH_HAS_NEON
        return from_neon(vabsq_f32(to_neon(*this)));
#else
        float f[4];
        store_scalar(f, *this);
        const float out[4] = {std::fabs(f[0]), std::fabs(f[1]), std::fabs(f[2]), std::fabs(f[3])};
        return load_scalar(out);
#endif
    }

    f32x4 f32x4::min() const noexcept
    {
        float f[4];
        store_scalar(f, *this);
        float m = f[0];
        m = (f[1] < m) ? f[1] : m;
        m = (f[2] < m) ? f[2] : m;
        m = (f[3] < m) ? f[3] : m;
        return splat(m);
    }

    f32x4 f32x4::max() const noexcept
    {
        float f[4];
        store_scalar(f, *this);
        float m = f[0];
        m = (f[1] > m) ? f[1] : m;
        m = (f[2] > m) ? f[2] : m;
        m = (f[3] > m) ? f[3] : m;
        return splat(m);
    }

    f32x4 f32x4::min(f32x4 other) const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128(_mm_min_ps(to_m128(*this), to_m128(other)));
#elif CATALYST_MATH_HAS_NEON
        return from_neon(vminq_f32(to_neon(*this), to_neon(other)));
#else
        float a[4], b[4];
        store_scalar(a, *this);
        store_scalar(b, other);
        const float out[4] = {
            (a[0] < b[0]) ? a[0] : b[0],
            (a[1] < b[1]) ? a[1] : b[1],
            (a[2] < b[2]) ? a[2] : b[2],
            (a[3] < b[3]) ? a[3] : b[3],
        };
        return load_scalar(out);
#endif
    }

    f32x4 f32x4::max(f32x4 other) const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128(_mm_max_ps(to_m128(*this), to_m128(other)));
#elif CATALYST_MATH_HAS_NEON
        return from_neon(vmaxq_f32(to_neon(*this), to_neon(other)));
#else
        float a[4], b[4];
        store_scalar(a, *this);
        store_scalar(b, other);
        const float out[4] = {
            (a[0] > b[0]) ? a[0] : b[0],
            (a[1] > b[1]) ? a[1] : b[1],
            (a[2] > b[2]) ? a[2] : b[2],
            (a[3] > b[3]) ? a[3] : b[3],
        };
        return load_scalar(out);
#endif
    }

    mask32x4 f32x4::mask() const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        const __m128 x = to_m128(*this);
        const __m128 z = _mm_setzero_ps();
        return from_m128_mask(_mm_cmplt_ps(x, z));
#elif CATALYST_MATH_HAS_NEON
        const float32x4_t x = to_neon(*this);
        const float32x4_t z = vdupq_n_f32(0.0f);
        const uint32x4_t m = vcltq_f32(x, z);
        return from_neon_mask32(m);
#else
        float f[4];
        store_scalar(f, *this);
        const std::uint32_t m0 = (f[0] < 0.0f) ? 0xFFFFFFFFu : 0u;
        const std::uint32_t m1 = (f[1] < 0.0f) ? 0xFFFFFFFFu : 0u;
        const std::uint32_t m2 = (f[2] < 0.0f) ? 0xFFFFFFFFu : 0u;
        const std::uint32_t m3 = (f[3] < 0.0f) ? 0xFFFFFFFFu : 0u;
        const std::uint32_t out_u32[4] = {m0, m1, m2, m3};
        mask32x4 out;
        std::memcpy(detail::mask32x4_access::data(out), out_u32, 16);
        return out;
#endif
    }

    // ---- mask32x4 ----

    mask32x4::mask32x4() noexcept
    {
        std::memset(detail::mask32x4_access::data(*this), 0, 16);
    }

    mask32x4 mask32x4::all_false() noexcept
    {
        mask32x4 v;
        std::memset(detail::mask32x4_access::data(v), 0, 16);
        return v;
    }

    mask32x4 mask32x4::all_true() noexcept
    {
        mask32x4 v;
        const std::uint32_t bits[4] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
        std::memcpy(detail::mask32x4_access::data(v), bits, 16);
        return v;
    }

    mask32x4 mask32x4::from_bits(std::uint32_t b0, std::uint32_t b1, std::uint32_t b2, std::uint32_t b3) noexcept
    {
        mask32x4 v;
        const std::uint32_t bits[4] = {b0, b1, b2, b3};
        std::memcpy(detail::mask32x4_access::data(v), bits, 16);
        return v;
    }

    void mask32x4::store_unaligned(std::uint32_t *ptr) const noexcept
    {
        std::memcpy(ptr, detail::mask32x4_access::data(*this), 16);
    }

    mask32x4 operator&(mask32x4 a, mask32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128_mask(_mm_and_ps(to_m128_mask(a), to_m128_mask(b)));
#elif CATALYST_MATH_HAS_NEON
        return from_neon_mask32(vandq_u32(to_neon_mask32(a), to_neon_mask32(b)));
#else
        std::uint32_t av[4], bv[4];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return mask32x4::from_bits(av[0] & bv[0], av[1] & bv[1], av[2] & bv[2], av[3] & bv[3]);
#endif
    }

    mask32x4 operator|(mask32x4 a, mask32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128_mask(_mm_or_ps(to_m128_mask(a), to_m128_mask(b)));
#elif CATALYST_MATH_HAS_NEON
        return from_neon_mask32(vorrq_u32(to_neon_mask32(a), to_neon_mask32(b)));
#else
        std::uint32_t av[4], bv[4];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return mask32x4::from_bits(av[0] | bv[0], av[1] | bv[1], av[2] | bv[2], av[3] | bv[3]);
#endif
    }

    mask32x4 operator^(mask32x4 a, mask32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128_mask(_mm_xor_ps(to_m128_mask(a), to_m128_mask(b)));
#elif CATALYST_MATH_HAS_NEON
        return from_neon_mask32(veorq_u32(to_neon_mask32(a), to_neon_mask32(b)));
#else
        std::uint32_t av[4], bv[4];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return mask32x4::from_bits(av[0] ^ bv[0], av[1] ^ bv[1], av[2] ^ bv[2], av[3] ^ bv[3]);
#endif
    }

    mask32x4 operator~(mask32x4 a) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        const __m128 all_ones = _mm_castsi128_ps(_mm_set1_epi32(-1));
        return from_m128_mask(_mm_xor_ps(to_m128_mask(a), all_ones));
#elif CATALYST_MATH_HAS_NEON
        return from_neon_mask32(vmvnq_u32(to_neon_mask32(a)));
#else
        std::uint32_t av[4];
        a.store_unaligned(av);
        return mask32x4::from_bits(~av[0], ~av[1], ~av[2], ~av[3]);
#endif
    }

    mask32x4 &mask32x4::operator&=(mask32x4 other) noexcept
    {
        *this = *this & other;
        return *this;
    }

    mask32x4 &mask32x4::operator|=(mask32x4 other) noexcept
    {
        *this = *this | other;
        return *this;
    }

    mask32x4 &mask32x4::operator^=(mask32x4 other) noexcept
    {
        *this = *this ^ other;
        return *this;
    }

    f32x4 f32x4::sqrt() const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128(_mm_sqrt_ps(to_m128(*this)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        return from_neon(vsqrtq_f32(to_neon(*this)));
#else
        float f[4];
        store_scalar(f, *this);
        const float out[4] = {std::sqrt(f[0]), std::sqrt(f[1]), std::sqrt(f[2]), std::sqrt(f[3])};
        return load_scalar(out);
#endif
    }

    f32x4 f32x4::reciprocal() const noexcept
    {
        return splat(1.0f) / *this;
    }

    f32x4 f32x4::rsqrt() const noexcept
    {
        return splat(1.0f) / this->sqrt();
    }

    // ---- i32x4 ----

    i32x4::i32x4() noexcept
    {
        std::memset(detail::i32x4_access::data(*this), 0, 16);
    }

    i32x4 i32x4::zero() noexcept
    {
        i32x4 v;
        std::memset(detail::i32x4_access::data(v), 0, 16);
        return v;
    }

    i32x4 i32x4::set(std::int32_t x0, std::int32_t x1, std::int32_t x2, std::int32_t x3) noexcept
    {
        const std::int32_t tmp[4] = {x0, x1, x2, x3};
        return load_unaligned(tmp);
    }

    i32x4 i32x4::splat(std::int32_t x) noexcept
    {
        return set(x, x, x, x);
    }

    i32x4 i32x4::load_aligned(const std::int32_t *ptr) noexcept
    {
        i32x4 v;
#if CATALYST_MATH_HAS_SSE2
        const __m128i r = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr));
        std::memcpy(detail::i32x4_access::data(v), &r, 16);
#elif CATALYST_MATH_HAS_NEON
        const int32x4_t r = vld1q_s32(ptr);
        std::memcpy(detail::i32x4_access::data(v), &r, 16);
#else
        std::memcpy(detail::i32x4_access::data(v), ptr, 16);
#endif
        return v;
    }

    i32x4 i32x4::load_unaligned(const std::int32_t *ptr) noexcept
    {
        i32x4 v;
#if CATALYST_MATH_HAS_SSE2
        const __m128i r = _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr));
        std::memcpy(detail::i32x4_access::data(v), &r, 16);
#elif CATALYST_MATH_HAS_NEON
        const int32x4_t r = vld1q_s32(ptr);
        std::memcpy(detail::i32x4_access::data(v), &r, 16);
#else
        std::memcpy(detail::i32x4_access::data(v), ptr, 16);
#endif
        return v;
    }

    void i32x4::store_aligned(std::int32_t *ptr) const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        const __m128i r = to_m128i(*this);
        _mm_store_si128(reinterpret_cast<__m128i *>(ptr), r);
#elif CATALYST_MATH_HAS_NEON
        const int32x4_t r = to_neon_i32(*this);
        vst1q_s32(ptr, r);
#else
        std::memcpy(ptr, detail::i32x4_access::data(*this), 16);
#endif
    }

    void i32x4::store_unaligned(std::int32_t *ptr) const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        const __m128i r = to_m128i(*this);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(ptr), r);
#elif CATALYST_MATH_HAS_NEON
        const int32x4_t r = to_neon_i32(*this);
        vst1q_s32(ptr, r);
#else
        std::memcpy(ptr, detail::i32x4_access::data(*this), 16);
#endif
    }

    i32x4 operator+(i32x4 a, i32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_i32(_mm_add_epi32(to_m128i(a), to_m128i(b)));
#elif CATALYST_MATH_HAS_NEON
        return from_neon_i32(vaddq_s32(to_neon_i32(a), to_neon_i32(b)));
#else
        std::int32_t av[4], bv[4];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return i32x4::set(av[0] + bv[0], av[1] + bv[1], av[2] + bv[2], av[3] + bv[3]);
#endif
    }

    i32x4 operator-(i32x4 a, i32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_i32(_mm_sub_epi32(to_m128i(a), to_m128i(b)));
#elif CATALYST_MATH_HAS_NEON
        return from_neon_i32(vsubq_s32(to_neon_i32(a), to_neon_i32(b)));
#else
        std::int32_t av[4], bv[4];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return i32x4::set(av[0] - bv[0], av[1] - bv[1], av[2] - bv[2], av[3] - bv[3]);
#endif
    }

    i32x4 operator&(i32x4 a, i32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_i32(_mm_and_si128(to_m128i(a), to_m128i(b)));
#elif CATALYST_MATH_HAS_NEON
        return from_neon_i32(vreinterpretq_s32_u32(vandq_u32(vreinterpretq_u32_s32(to_neon_i32(a)), vreinterpretq_u32_s32(to_neon_i32(b)))));
#else
        std::int32_t av[4], bv[4];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return i32x4::set(av[0] & bv[0], av[1] & bv[1], av[2] & bv[2], av[3] & bv[3]);
#endif
    }

    i32x4 operator|(i32x4 a, i32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_i32(_mm_or_si128(to_m128i(a), to_m128i(b)));
#elif CATALYST_MATH_HAS_NEON
        return from_neon_i32(vreinterpretq_s32_u32(vorrq_u32(vreinterpretq_u32_s32(to_neon_i32(a)), vreinterpretq_u32_s32(to_neon_i32(b)))));
#else
        std::int32_t av[4], bv[4];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return i32x4::set(av[0] | bv[0], av[1] | bv[1], av[2] | bv[2], av[3] | bv[3]);
#endif
    }

    i32x4 operator^(i32x4 a, i32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_i32(_mm_xor_si128(to_m128i(a), to_m128i(b)));
#elif CATALYST_MATH_HAS_NEON
        return from_neon_i32(vreinterpretq_s32_u32(veorq_u32(vreinterpretq_u32_s32(to_neon_i32(a)), vreinterpretq_u32_s32(to_neon_i32(b)))));
#else
        std::int32_t av[4], bv[4];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return i32x4::set(av[0] ^ bv[0], av[1] ^ bv[1], av[2] ^ bv[2], av[3] ^ bv[3]);
#endif
    }

    i32x4 operator~(i32x4 a) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_i32(_mm_xor_si128(to_m128i(a), _mm_set1_epi32(-1)));
#elif CATALYST_MATH_HAS_NEON
        const uint32x4_t ua = vreinterpretq_u32_s32(to_neon_i32(a));
        return from_neon_i32(vreinterpretq_s32_u32(vmvnq_u32(ua)));
#else
        std::int32_t av[4];
        a.store_unaligned(av);
        return i32x4::set(~av[0], ~av[1], ~av[2], ~av[3]);
#endif
    }

    i32x4 &i32x4::operator+=(i32x4 other) noexcept
    {
        *this = *this + other;
        return *this;
    }

    i32x4 &i32x4::operator-=(i32x4 other) noexcept
    {
        *this = *this - other;
        return *this;
    }

    i32x4 &i32x4::operator&=(i32x4 other) noexcept
    {
        *this = *this & other;
        return *this;
    }

    i32x4 &i32x4::operator|=(i32x4 other) noexcept
    {
        *this = *this | other;
        return *this;
    }

    i32x4 &i32x4::operator^=(i32x4 other) noexcept
    {
        *this = *this ^ other;
        return *this;
    }

    // ---- u32x4 ----

    u32x4::u32x4() noexcept
    {
        std::memset(detail::u32x4_access::data(*this), 0, 16);
    }

    u32x4 u32x4::zero() noexcept
    {
        u32x4 v;
        std::memset(detail::u32x4_access::data(v), 0, 16);
        return v;
    }

    u32x4 u32x4::set(std::uint32_t x0, std::uint32_t x1, std::uint32_t x2, std::uint32_t x3) noexcept
    {
        const std::uint32_t tmp[4] = {x0, x1, x2, x3};
        return load_unaligned(tmp);
    }

    u32x4 u32x4::splat(std::uint32_t x) noexcept
    {
        return set(x, x, x, x);
    }

    u32x4 u32x4::load_aligned(const std::uint32_t *ptr) noexcept
    {
        u32x4 v;
#if CATALYST_MATH_HAS_SSE2
        const __m128i r = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr));
        std::memcpy(detail::u32x4_access::data(v), &r, 16);
#elif CATALYST_MATH_HAS_NEON
        const uint32x4_t r = vld1q_u32(ptr);
        std::memcpy(detail::u32x4_access::data(v), &r, 16);
#else
        std::memcpy(detail::u32x4_access::data(v), ptr, 16);
#endif
        return v;
    }

    u32x4 u32x4::load_unaligned(const std::uint32_t *ptr) noexcept
    {
        u32x4 v;
#if CATALYST_MATH_HAS_SSE2
        const __m128i r = _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr));
        std::memcpy(detail::u32x4_access::data(v), &r, 16);
#elif CATALYST_MATH_HAS_NEON
        const uint32x4_t r = vld1q_u32(ptr);
        std::memcpy(detail::u32x4_access::data(v), &r, 16);
#else
        std::memcpy(detail::u32x4_access::data(v), ptr, 16);
#endif
        return v;
    }

    void u32x4::store_aligned(std::uint32_t *ptr) const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        const __m128i r = to_m128i(*this);
        _mm_store_si128(reinterpret_cast<__m128i *>(ptr), r);
#elif CATALYST_MATH_HAS_NEON
        const uint32x4_t r = to_neon_u32(*this);
        vst1q_u32(ptr, r);
#else
        std::memcpy(ptr, detail::u32x4_access::data(*this), 16);
#endif
    }

    void u32x4::store_unaligned(std::uint32_t *ptr) const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        const __m128i r = to_m128i(*this);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(ptr), r);
#elif CATALYST_MATH_HAS_NEON
        const uint32x4_t r = to_neon_u32(*this);
        vst1q_u32(ptr, r);
#else
        std::memcpy(ptr, detail::u32x4_access::data(*this), 16);
#endif
    }

    u32x4 operator+(u32x4 a, u32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_u32(_mm_add_epi32(to_m128i(a), to_m128i(b)));
#elif CATALYST_MATH_HAS_NEON
        return from_neon_u32(vaddq_u32(to_neon_u32(a), to_neon_u32(b)));
#else
        std::uint32_t av[4], bv[4];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return u32x4::set(av[0] + bv[0], av[1] + bv[1], av[2] + bv[2], av[3] + bv[3]);
#endif
    }

    u32x4 operator-(u32x4 a, u32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_u32(_mm_sub_epi32(to_m128i(a), to_m128i(b)));
#elif CATALYST_MATH_HAS_NEON
        return from_neon_u32(vsubq_u32(to_neon_u32(a), to_neon_u32(b)));
#else
        std::uint32_t av[4], bv[4];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return u32x4::set(av[0] - bv[0], av[1] - bv[1], av[2] - bv[2], av[3] - bv[3]);
#endif
    }

    u32x4 operator&(u32x4 a, u32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_u32(_mm_and_si128(to_m128i(a), to_m128i(b)));
#elif CATALYST_MATH_HAS_NEON
        return from_neon_u32(vandq_u32(to_neon_u32(a), to_neon_u32(b)));
#else
        std::uint32_t av[4], bv[4];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return u32x4::set(av[0] & bv[0], av[1] & bv[1], av[2] & bv[2], av[3] & bv[3]);
#endif
    }

    u32x4 operator|(u32x4 a, u32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_u32(_mm_or_si128(to_m128i(a), to_m128i(b)));
#elif CATALYST_MATH_HAS_NEON
        return from_neon_u32(vorrq_u32(to_neon_u32(a), to_neon_u32(b)));
#else
        std::uint32_t av[4], bv[4];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return u32x4::set(av[0] | bv[0], av[1] | bv[1], av[2] | bv[2], av[3] | bv[3]);
#endif
    }

    u32x4 operator^(u32x4 a, u32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_u32(_mm_xor_si128(to_m128i(a), to_m128i(b)));
#elif CATALYST_MATH_HAS_NEON
        return from_neon_u32(veorq_u32(to_neon_u32(a), to_neon_u32(b)));
#else
        std::uint32_t av[4], bv[4];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return u32x4::set(av[0] ^ bv[0], av[1] ^ bv[1], av[2] ^ bv[2], av[3] ^ bv[3]);
#endif
    }

    u32x4 operator~(u32x4 a) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_u32(_mm_xor_si128(to_m128i(a), _mm_set1_epi32(-1)));
#elif CATALYST_MATH_HAS_NEON
        return from_neon_u32(vmvnq_u32(to_neon_u32(a)));
#else
        std::uint32_t av[4];
        a.store_unaligned(av);
        return u32x4::set(~av[0], ~av[1], ~av[2], ~av[3]);
#endif
    }

    u32x4 &u32x4::operator+=(u32x4 other) noexcept
    {
        *this = *this + other;
        return *this;
    }

    u32x4 &u32x4::operator-=(u32x4 other) noexcept
    {
        *this = *this - other;
        return *this;
    }

    u32x4 &u32x4::operator&=(u32x4 other) noexcept
    {
        *this = *this & other;
        return *this;
    }

    u32x4 &u32x4::operator|=(u32x4 other) noexcept
    {
        *this = *this | other;
        return *this;
    }

    u32x4 &u32x4::operator^=(u32x4 other) noexcept
    {
        *this = *this ^ other;
        return *this;
    }

    // ---- f64x2 ----

    f64x2::f64x2() noexcept
    {
        std::memset(detail::f64x2_access::data(*this), 0, 16);
    }

    f64x2 f64x2::zero() noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128d(_mm_setzero_pd());
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        return from_neon_f64(vdupq_n_f64(0.0));
#else
        return f64x2::splat(0.0);
#endif
    }

    f64x2 f64x2::set(double x0, double x1) noexcept
    {
        const double tmp[2] = {x0, x1};
        return load_unaligned(tmp);
    }

    f64x2 f64x2::splat(double x) noexcept
    {
        return set(x, x);
    }

    f64x2 f64x2::load_aligned(const double *ptr) noexcept
    {
        f64x2 v;
#if CATALYST_MATH_HAS_SSE2
        const __m128d r = _mm_load_pd(ptr);
        std::memcpy(detail::f64x2_access::data(v), &r, 16);
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const float64x2_t r = vld1q_f64(ptr);
        std::memcpy(detail::f64x2_access::data(v), &r, 16);
#else
        std::memcpy(detail::f64x2_access::data(v), ptr, 16);
#endif
        return v;
    }

    f64x2 f64x2::load_unaligned(const double *ptr) noexcept
    {
        f64x2 v;
#if CATALYST_MATH_HAS_SSE2
        const __m128d r = _mm_loadu_pd(ptr);
        std::memcpy(detail::f64x2_access::data(v), &r, 16);
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const float64x2_t r = vld1q_f64(ptr);
        std::memcpy(detail::f64x2_access::data(v), &r, 16);
#else
        std::memcpy(detail::f64x2_access::data(v), ptr, 16);
#endif
        return v;
    }

    void f64x2::store_aligned(double *ptr) const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        const __m128d r = to_m128d(*this);
        _mm_store_pd(ptr, r);
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const float64x2_t r = to_neon_f64(*this);
        vst1q_f64(ptr, r);
#else
        std::memcpy(ptr, detail::f64x2_access::data(*this), 16);
#endif
    }

    void f64x2::store_unaligned(double *ptr) const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        const __m128d r = to_m128d(*this);
        _mm_storeu_pd(ptr, r);
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const float64x2_t r = to_neon_f64(*this);
        vst1q_f64(ptr, r);
#else
        std::memcpy(ptr, detail::f64x2_access::data(*this), 16);
#endif
    }

    f64x2 operator+(f64x2 a, f64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128d(_mm_add_pd(to_m128d(a), to_m128d(b)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        return from_neon_f64(vaddq_f64(to_neon_f64(a), to_neon_f64(b)));
#else
        double av[2], bv[2];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return f64x2::set(av[0] + bv[0], av[1] + bv[1]);
#endif
    }

    f64x2 operator-(f64x2 a, f64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128d(_mm_sub_pd(to_m128d(a), to_m128d(b)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        return from_neon_f64(vsubq_f64(to_neon_f64(a), to_neon_f64(b)));
#else
        double av[2], bv[2];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return f64x2::set(av[0] - bv[0], av[1] - bv[1]);
#endif
    }

    f64x2 operator*(f64x2 a, f64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128d(_mm_mul_pd(to_m128d(a), to_m128d(b)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        return from_neon_f64(vmulq_f64(to_neon_f64(a), to_neon_f64(b)));
#else
        double av[2], bv[2];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return f64x2::set(av[0] * bv[0], av[1] * bv[1]);
#endif
    }

    f64x2 operator/(f64x2 a, f64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128d(_mm_div_pd(to_m128d(a), to_m128d(b)));
#else
        double av[2], bv[2];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return f64x2::set(av[0] / bv[0], av[1] / bv[1]);
#endif
    }

    f64x2 &f64x2::operator+=(f64x2 other) noexcept
    {
        *this = *this + other;
        return *this;
    }

    f64x2 &f64x2::operator-=(f64x2 other) noexcept
    {
        *this = *this - other;
        return *this;
    }

    f64x2 &f64x2::operator*=(f64x2 other) noexcept
    {
        *this = *this * other;
        return *this;
    }

    f64x2 &f64x2::operator/=(f64x2 other) noexcept
    {
        *this = *this / other;
        return *this;
    }

    f64x2 f64x2::abs() const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        const __m128d x = to_m128d(*this);
        const __m128d sign_mask = _mm_set1_pd(-0.0);
        return from_m128d(_mm_andnot_pd(sign_mask, x));
#else
        double v[2];
        store_unaligned(v);
        return set(std::fabs(v[0]), std::fabs(v[1]));
#endif
    }

    f64x2 f64x2::min(f64x2 other) const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128d(_mm_min_pd(to_m128d(*this), to_m128d(other)));
#else
        double a[2], b[2];
        store_unaligned(a);
        other.store_unaligned(b);
        return set((a[0] < b[0]) ? a[0] : b[0], (a[1] < b[1]) ? a[1] : b[1]);
#endif
    }

    f64x2 f64x2::max(f64x2 other) const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128d(_mm_max_pd(to_m128d(*this), to_m128d(other)));
#else
        double a[2], b[2];
        store_unaligned(a);
        other.store_unaligned(b);
        return set((a[0] > b[0]) ? a[0] : b[0], (a[1] > b[1]) ? a[1] : b[1]);
#endif
    }

    f64x2 f64x2::sqrt() const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128d(_mm_sqrt_pd(to_m128d(*this)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        return from_neon_f64(vsqrtq_f64(to_neon_f64(*this)));
#else
        double v[2];
        store_unaligned(v);
        return set(std::sqrt(v[0]), std::sqrt(v[1]));
#endif
    }

    f64x2 f64x2::reciprocal() const noexcept
    {
        return splat(1.0) / *this;
    }

    f64x2 f64x2::rsqrt() const noexcept
    {
        return splat(1.0) / this->sqrt();
    }

    // ---- i64x2 ----

    i64x2::i64x2() noexcept
    {
        std::memset(detail::i64x2_access::data(*this), 0, 16);
    }

    i64x2 i64x2::zero() noexcept
    {
        i64x2 v;
        std::memset(detail::i64x2_access::data(v), 0, 16);
        return v;
    }

    i64x2 i64x2::set(std::int64_t x0, std::int64_t x1) noexcept
    {
        const std::int64_t tmp[2] = {x0, x1};
        return load_unaligned(tmp);
    }

    i64x2 i64x2::splat(std::int64_t x) noexcept
    {
        return set(x, x);
    }

    i64x2 i64x2::load_aligned(const std::int64_t *ptr) noexcept
    {
        i64x2 v;
#if CATALYST_MATH_HAS_SSE2
        const __m128i r = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr));
        std::memcpy(detail::i64x2_access::data(v), &r, 16);
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const int64x2_t r = vld1q_s64(ptr);
        std::memcpy(detail::i64x2_access::data(v), &r, 16);
#else
        std::memcpy(detail::i64x2_access::data(v), ptr, 16);
#endif
        return v;
    }

    i64x2 i64x2::load_unaligned(const std::int64_t *ptr) noexcept
    {
        i64x2 v;
#if CATALYST_MATH_HAS_SSE2
        const __m128i r = _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr));
        std::memcpy(detail::i64x2_access::data(v), &r, 16);
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const int64x2_t r = vld1q_s64(ptr);
        std::memcpy(detail::i64x2_access::data(v), &r, 16);
#else
        std::memcpy(detail::i64x2_access::data(v), ptr, 16);
#endif
        return v;
    }

    void i64x2::store_aligned(std::int64_t *ptr) const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        const __m128i r = to_m128i(*this);
        _mm_store_si128(reinterpret_cast<__m128i *>(ptr), r);
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const int64x2_t r = to_neon_i64(*this);
        vst1q_s64(ptr, r);
#else
        std::memcpy(ptr, detail::i64x2_access::data(*this), 16);
#endif
    }

    void i64x2::store_unaligned(std::int64_t *ptr) const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        const __m128i r = to_m128i(*this);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(ptr), r);
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const int64x2_t r = to_neon_i64(*this);
        vst1q_s64(ptr, r);
#else
        std::memcpy(ptr, detail::i64x2_access::data(*this), 16);
#endif
    }

    i64x2 operator+(i64x2 a, i64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_i64(_mm_add_epi64(to_m128i(a), to_m128i(b)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        return from_neon_i64(vaddq_s64(to_neon_i64(a), to_neon_i64(b)));
#else
        std::int64_t av[2], bv[2];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return i64x2::set(av[0] + bv[0], av[1] + bv[1]);
#endif
    }

    i64x2 operator-(i64x2 a, i64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_i64(_mm_sub_epi64(to_m128i(a), to_m128i(b)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        return from_neon_i64(vsubq_s64(to_neon_i64(a), to_neon_i64(b)));
#else
        std::int64_t av[2], bv[2];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return i64x2::set(av[0] - bv[0], av[1] - bv[1]);
#endif
    }

    i64x2 operator&(i64x2 a, i64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_i64(_mm_and_si128(to_m128i(a), to_m128i(b)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const uint64x2_t ua = vreinterpretq_u64_s64(to_neon_i64(a));
        const uint64x2_t ub = vreinterpretq_u64_s64(to_neon_i64(b));
        return from_neon_i64(vreinterpretq_s64_u64(vandq_u64(ua, ub)));
#else
        std::int64_t av[2], bv[2];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return i64x2::set(av[0] & bv[0], av[1] & bv[1]);
#endif
    }

    i64x2 operator|(i64x2 a, i64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_i64(_mm_or_si128(to_m128i(a), to_m128i(b)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const uint64x2_t ua = vreinterpretq_u64_s64(to_neon_i64(a));
        const uint64x2_t ub = vreinterpretq_u64_s64(to_neon_i64(b));
        return from_neon_i64(vreinterpretq_s64_u64(vorrq_u64(ua, ub)));
#else
        std::int64_t av[2], bv[2];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return i64x2::set(av[0] | bv[0], av[1] | bv[1]);
#endif
    }

    i64x2 operator^(i64x2 a, i64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_i64(_mm_xor_si128(to_m128i(a), to_m128i(b)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const uint64x2_t ua = vreinterpretq_u64_s64(to_neon_i64(a));
        const uint64x2_t ub = vreinterpretq_u64_s64(to_neon_i64(b));
        return from_neon_i64(vreinterpretq_s64_u64(veorq_u64(ua, ub)));
#else
        std::int64_t av[2], bv[2];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return i64x2::set(av[0] ^ bv[0], av[1] ^ bv[1]);
#endif
    }

    i64x2 operator~(i64x2 a) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_i64(_mm_xor_si128(to_m128i(a), _mm_set1_epi32(-1)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const uint64x2_t ua = vreinterpretq_u64_s64(to_neon_i64(a));
        return from_neon_i64(vreinterpretq_s64_u64(vmvnq_u64(ua)));
#else
        std::int64_t av[2];
        a.store_unaligned(av);
        return i64x2::set(~av[0], ~av[1]);
#endif
    }

    i64x2 &i64x2::operator+=(i64x2 other) noexcept
    {
        *this = *this + other;
        return *this;
    }

    i64x2 &i64x2::operator-=(i64x2 other) noexcept
    {
        *this = *this - other;
        return *this;
    }

    i64x2 &i64x2::operator&=(i64x2 other) noexcept
    {
        *this = *this & other;
        return *this;
    }

    i64x2 &i64x2::operator|=(i64x2 other) noexcept
    {
        *this = *this | other;
        return *this;
    }

    i64x2 &i64x2::operator^=(i64x2 other) noexcept
    {
        *this = *this ^ other;
        return *this;
    }

    // ---- u64x2 ----

    u64x2::u64x2() noexcept
    {
        std::memset(detail::u64x2_access::data(*this), 0, 16);
    }

    u64x2 u64x2::zero() noexcept
    {
        u64x2 v;
        std::memset(detail::u64x2_access::data(v), 0, 16);
        return v;
    }

    u64x2 u64x2::set(std::uint64_t x0, std::uint64_t x1) noexcept
    {
        const std::uint64_t tmp[2] = {x0, x1};
        return load_unaligned(tmp);
    }

    u64x2 u64x2::splat(std::uint64_t x) noexcept
    {
        return set(x, x);
    }

    u64x2 u64x2::load_aligned(const std::uint64_t *ptr) noexcept
    {
        u64x2 v;
#if CATALYST_MATH_HAS_SSE2
        const __m128i r = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr));
        std::memcpy(detail::u64x2_access::data(v), &r, 16);
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const uint64x2_t r = vld1q_u64(ptr);
        std::memcpy(detail::u64x2_access::data(v), &r, 16);
#else
        std::memcpy(detail::u64x2_access::data(v), ptr, 16);
#endif
        return v;
    }

    u64x2 u64x2::load_unaligned(const std::uint64_t *ptr) noexcept
    {
        u64x2 v;
#if CATALYST_MATH_HAS_SSE2
        const __m128i r = _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr));
        std::memcpy(detail::u64x2_access::data(v), &r, 16);
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const uint64x2_t r = vld1q_u64(ptr);
        std::memcpy(detail::u64x2_access::data(v), &r, 16);
#else
        std::memcpy(detail::u64x2_access::data(v), ptr, 16);
#endif
        return v;
    }

    void u64x2::store_aligned(std::uint64_t *ptr) const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        const __m128i r = to_m128i(*this);
        _mm_store_si128(reinterpret_cast<__m128i *>(ptr), r);
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const uint64x2_t r = to_neon_u64(*this);
        vst1q_u64(ptr, r);
#else
        std::memcpy(ptr, detail::u64x2_access::data(*this), 16);
#endif
    }

    void u64x2::store_unaligned(std::uint64_t *ptr) const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        const __m128i r = to_m128i(*this);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(ptr), r);
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const uint64x2_t r = to_neon_u64(*this);
        vst1q_u64(ptr, r);
#else
        std::memcpy(ptr, detail::u64x2_access::data(*this), 16);
#endif
    }

    u64x2 operator+(u64x2 a, u64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_u64(_mm_add_epi64(to_m128i(a), to_m128i(b)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        return from_neon_u64(vaddq_u64(to_neon_u64(a), to_neon_u64(b)));
#else
        std::uint64_t av[2], bv[2];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return u64x2::set(av[0] + bv[0], av[1] + bv[1]);
#endif
    }

    u64x2 operator-(u64x2 a, u64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_u64(_mm_sub_epi64(to_m128i(a), to_m128i(b)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        return from_neon_u64(vsubq_u64(to_neon_u64(a), to_neon_u64(b)));
#else
        std::uint64_t av[2], bv[2];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return u64x2::set(av[0] - bv[0], av[1] - bv[1]);
#endif
    }

    u64x2 operator&(u64x2 a, u64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_u64(_mm_and_si128(to_m128i(a), to_m128i(b)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        return from_neon_u64(vandq_u64(to_neon_u64(a), to_neon_u64(b)));
#else
        std::uint64_t av[2], bv[2];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return u64x2::set(av[0] & bv[0], av[1] & bv[1]);
#endif
    }

    u64x2 operator|(u64x2 a, u64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_u64(_mm_or_si128(to_m128i(a), to_m128i(b)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        return from_neon_u64(vorrq_u64(to_neon_u64(a), to_neon_u64(b)));
#else
        std::uint64_t av[2], bv[2];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return u64x2::set(av[0] | bv[0], av[1] | bv[1]);
#endif
    }

    u64x2 operator^(u64x2 a, u64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_u64(_mm_xor_si128(to_m128i(a), to_m128i(b)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        return from_neon_u64(veorq_u64(to_neon_u64(a), to_neon_u64(b)));
#else
        std::uint64_t av[2], bv[2];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return u64x2::set(av[0] ^ bv[0], av[1] ^ bv[1]);
#endif
    }

    u64x2 operator~(u64x2 a) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128i_u64(_mm_xor_si128(to_m128i(a), _mm_set1_epi32(-1)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        return from_neon_u64(vmvnq_u64(to_neon_u64(a)));
#else
        std::uint64_t av[2];
        a.store_unaligned(av);
        return u64x2::set(~av[0], ~av[1]);
#endif
    }

    u64x2 &u64x2::operator+=(u64x2 other) noexcept
    {
        *this = *this + other;
        return *this;
    }

    u64x2 &u64x2::operator-=(u64x2 other) noexcept
    {
        *this = *this - other;
        return *this;
    }

    u64x2 &u64x2::operator&=(u64x2 other) noexcept
    {
        *this = *this & other;
        return *this;
    }

    u64x2 &u64x2::operator|=(u64x2 other) noexcept
    {
        *this = *this | other;
        return *this;
    }

    u64x2 &u64x2::operator^=(u64x2 other) noexcept
    {
        *this = *this ^ other;
        return *this;
    }

    // ---- Comparisons / select ----

    mask32x4 cmp_eq(f32x4 a, f32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128_mask(_mm_cmpeq_ps(to_m128(a), to_m128(b)));
#elif CATALYST_MATH_HAS_NEON
        const uint32x4_t m = vceqq_f32(to_neon(a), to_neon(b));
        return from_neon_mask32(m);
#else
        float av[4], bv[4];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        const std::uint32_t m0 = (av[0] == bv[0]) ? 0xFFFFFFFFu : 0u;
        const std::uint32_t m1 = (av[1] == bv[1]) ? 0xFFFFFFFFu : 0u;
        const std::uint32_t m2 = (av[2] == bv[2]) ? 0xFFFFFFFFu : 0u;
        const std::uint32_t m3 = (av[3] == bv[3]) ? 0xFFFFFFFFu : 0u;
        const std::uint32_t out_u32[4] = {m0, m1, m2, m3};
        mask32x4 out;
        std::memcpy(detail::mask32x4_access::data(out), out_u32, 16);
        return out;
#endif
    }

    mask32x4 cmp_lt(f32x4 a, f32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128_mask(_mm_cmplt_ps(to_m128(a), to_m128(b)));
#elif CATALYST_MATH_HAS_NEON
        const uint32x4_t m = vcltq_f32(to_neon(a), to_neon(b));
        return from_neon_mask32(m);
#else
        float av[4], bv[4];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        const std::uint32_t m0 = (av[0] < bv[0]) ? 0xFFFFFFFFu : 0u;
        const std::uint32_t m1 = (av[1] < bv[1]) ? 0xFFFFFFFFu : 0u;
        const std::uint32_t m2 = (av[2] < bv[2]) ? 0xFFFFFFFFu : 0u;
        const std::uint32_t m3 = (av[3] < bv[3]) ? 0xFFFFFFFFu : 0u;
        const std::uint32_t out_u32[4] = {m0, m1, m2, m3};
        mask32x4 out;
        std::memcpy(detail::mask32x4_access::data(out), out_u32, 16);
        return out;
#endif
    }

    mask32x4 cmp_le(f32x4 a, f32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128_mask(_mm_cmple_ps(to_m128(a), to_m128(b)));
#elif CATALYST_MATH_HAS_NEON
        const uint32x4_t m = vcleq_f32(to_neon(a), to_neon(b));
        return from_neon_mask32(m);
#else
        float av[4], bv[4];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        const std::uint32_t m0 = (av[0] <= bv[0]) ? 0xFFFFFFFFu : 0u;
        const std::uint32_t m1 = (av[1] <= bv[1]) ? 0xFFFFFFFFu : 0u;
        const std::uint32_t m2 = (av[2] <= bv[2]) ? 0xFFFFFFFFu : 0u;
        const std::uint32_t m3 = (av[3] <= bv[3]) ? 0xFFFFFFFFu : 0u;
        const std::uint32_t out_u32[4] = {m0, m1, m2, m3};
                mask32x4 out;
                std::memcpy(detail::mask32x4_access::data(out), out_u32, 16);
        return out;
#endif
    }

        mask32x4 cmp_gt(f32x4 a, f32x4 b) noexcept
    {
        return cmp_lt(b, a);
    }

        mask32x4 cmp_ge(f32x4 a, f32x4 b) noexcept
    {
        return cmp_le(b, a);
    }

        f32x4 select(mask32x4 mask, f32x4 a, f32x4 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
                const __m128 m = to_m128_mask(mask);
        const __m128 x = to_m128(a);
        const __m128 y = to_m128(b);
        return from_m128(_mm_or_ps(_mm_and_ps(m, x), _mm_andnot_ps(m, y)));
#elif CATALYST_MATH_HAS_NEON
                const uint32x4_t m = to_neon_mask32(mask);
        const uint32x4_t x = vreinterpretq_u32_f32(to_neon(a));
        const uint32x4_t y = vreinterpretq_u32_f32(to_neon(b));
        return from_neon(vreinterpretq_f32_u32(vbslq_u32(m, x, y)));
#else
        std::uint32_t mb[4], xb[4], yb[4];
                mask.store_unaligned(mb);
        std::memcpy(xb, detail::f32x4_access::data(a), 16);
        std::memcpy(yb, detail::f32x4_access::data(b), 16);
        const std::uint32_t out_u32[4] = {
            (mb[0] & xb[0]) | (~mb[0] & yb[0]),
            (mb[1] & xb[1]) | (~mb[1] & yb[1]),
            (mb[2] & xb[2]) | (~mb[2] & yb[2]),
            (mb[3] & xb[3]) | (~mb[3] & yb[3]),
        };
        f32x4 out;
        std::memcpy(detail::f32x4_access::data(out), out_u32, 16);
        return out;
#endif
    }

    mask64x2 cmp_eq(f64x2 a, f64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128d_mask(_mm_cmpeq_pd(to_m128d(a), to_m128d(b)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const uint64x2_t m = vceqq_f64(to_neon_f64(a), to_neon_f64(b));
        return from_neon_mask64(m);
#else
        double av[2], bv[2];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        const std::uint64_t m0 = (av[0] == bv[0]) ? 0xFFFFFFFFFFFFFFFFull : 0ull;
        const std::uint64_t m1 = (av[1] == bv[1]) ? 0xFFFFFFFFFFFFFFFFull : 0ull;
        const std::uint64_t out_u64[2] = {m0, m1};
        mask64x2 out;
        std::memcpy(detail::mask64x2_access::data(out), out_u64, 16);
        return out;
#endif
    }

    mask64x2 cmp_lt(f64x2 a, f64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128d_mask(_mm_cmplt_pd(to_m128d(a), to_m128d(b)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const uint64x2_t m = vcltq_f64(to_neon_f64(a), to_neon_f64(b));
        return from_neon_mask64(m);
#else
        double av[2], bv[2];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        const std::uint64_t m0 = (av[0] < bv[0]) ? 0xFFFFFFFFFFFFFFFFull : 0ull;
        const std::uint64_t m1 = (av[1] < bv[1]) ? 0xFFFFFFFFFFFFFFFFull : 0ull;
        const std::uint64_t out_u64[2] = {m0, m1};
        mask64x2 out;
        std::memcpy(detail::mask64x2_access::data(out), out_u64, 16);
        return out;
#endif
    }

    mask64x2 cmp_le(f64x2 a, f64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return from_m128d_mask(_mm_cmple_pd(to_m128d(a), to_m128d(b)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const uint64x2_t m = vcleq_f64(to_neon_f64(a), to_neon_f64(b));
        return from_neon_mask64(m);
#else
        double av[2], bv[2];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        const std::uint64_t m0 = (av[0] <= bv[0]) ? 0xFFFFFFFFFFFFFFFFull : 0ull;
        const std::uint64_t m1 = (av[1] <= bv[1]) ? 0xFFFFFFFFFFFFFFFFull : 0ull;
        const std::uint64_t out_u64[2] = {m0, m1};
                mask64x2 out;
                std::memcpy(detail::mask64x2_access::data(out), out_u64, 16);
        return out;
#endif
    }

        mask64x2 cmp_gt(f64x2 a, f64x2 b) noexcept
    {
        return cmp_lt(b, a);
    }

        mask64x2 cmp_ge(f64x2 a, f64x2 b) noexcept
    {
        return cmp_le(b, a);
    }

        f64x2 select(mask64x2 mask, f64x2 a, f64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
                const __m128d m = to_m128d_mask(mask);
        const __m128d x = to_m128d(a);
        const __m128d y = to_m128d(b);
        return from_m128d(_mm_or_pd(_mm_and_pd(m, x), _mm_andnot_pd(m, y)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
                const uint64x2_t m = to_neon_mask64(mask);
        const uint64x2_t x = vreinterpretq_u64_f64(to_neon_f64(a));
        const uint64x2_t y = vreinterpretq_u64_f64(to_neon_f64(b));
        return from_neon_f64(vreinterpretq_f64_u64(vbslq_u64(m, x, y)));
#else
        std::uint64_t mb[2], xb[2], yb[2];
                mask.store_unaligned(mb);
        std::memcpy(xb, detail::f64x2_access::data(a), 16);
        std::memcpy(yb, detail::f64x2_access::data(b), 16);
        const std::uint64_t out_u64[2] = {
            (mb[0] & xb[0]) | (~mb[0] & yb[0]),
            (mb[1] & xb[1]) | (~mb[1] & yb[1]),
        };
        f64x2 out;
        std::memcpy(detail::f64x2_access::data(out), out_u64, 16);
        return out;
#endif
    }

    // ---- mask64x2 ----

    mask64x2::mask64x2() noexcept
    {
        std::memset(detail::mask64x2_access::data(*this), 0, 16);
    }

    mask64x2 mask64x2::all_false() noexcept
    {
        mask64x2 v;
        std::memset(detail::mask64x2_access::data(v), 0, 16);
        return v;
    }

    mask64x2 mask64x2::all_true() noexcept
    {
        mask64x2 v;
        const std::uint64_t bits[2] = {0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull};
        std::memcpy(detail::mask64x2_access::data(v), bits, 16);
        return v;
    }

    mask64x2 mask64x2::from_bits(std::uint64_t b0, std::uint64_t b1) noexcept
    {
        mask64x2 v;
        const std::uint64_t bits[2] = {b0, b1};
        std::memcpy(detail::mask64x2_access::data(v), bits, 16);
        return v;
    }

    void mask64x2::store_unaligned(std::uint64_t *ptr) const noexcept
    {
        std::memcpy(ptr, detail::mask64x2_access::data(*this), 16);
    }

    mask64x2 operator&(mask64x2 a, mask64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        const __m128i ai = _mm_castpd_si128(to_m128d_mask(a));
        const __m128i bi = _mm_castpd_si128(to_m128d_mask(b));
        return from_m128d_mask(_mm_castsi128_pd(_mm_and_si128(ai, bi)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        return from_neon_mask64(vandq_u64(to_neon_mask64(a), to_neon_mask64(b)));
#else
        std::uint64_t av[2], bv[2];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return mask64x2::from_bits(av[0] & bv[0], av[1] & bv[1]);
#endif
    }

    mask64x2 operator|(mask64x2 a, mask64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        const __m128i ai = _mm_castpd_si128(to_m128d_mask(a));
        const __m128i bi = _mm_castpd_si128(to_m128d_mask(b));
        return from_m128d_mask(_mm_castsi128_pd(_mm_or_si128(ai, bi)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        return from_neon_mask64(vorrq_u64(to_neon_mask64(a), to_neon_mask64(b)));
#else
        std::uint64_t av[2], bv[2];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return mask64x2::from_bits(av[0] | bv[0], av[1] | bv[1]);
#endif
    }

    mask64x2 operator^(mask64x2 a, mask64x2 b) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        const __m128i ai = _mm_castpd_si128(to_m128d_mask(a));
        const __m128i bi = _mm_castpd_si128(to_m128d_mask(b));
        return from_m128d_mask(_mm_castsi128_pd(_mm_xor_si128(ai, bi)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        return from_neon_mask64(veorq_u64(to_neon_mask64(a), to_neon_mask64(b)));
#else
        std::uint64_t av[2], bv[2];
        a.store_unaligned(av);
        b.store_unaligned(bv);
        return mask64x2::from_bits(av[0] ^ bv[0], av[1] ^ bv[1]);
#endif
    }

    mask64x2 operator~(mask64x2 a) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        const __m128i ai = _mm_castpd_si128(to_m128d_mask(a));
        const __m128i all_ones = _mm_set1_epi32(-1);
        return from_m128d_mask(_mm_castsi128_pd(_mm_xor_si128(ai, all_ones)));
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        return from_neon_mask64(vmvnq_u64(to_neon_mask64(a)));
#else
        std::uint64_t av[2];
        a.store_unaligned(av);
        return mask64x2::from_bits(~av[0], ~av[1]);
#endif
    }

    mask64x2 &mask64x2::operator&=(mask64x2 other) noexcept
    {
        *this = *this & other;
        return *this;
    }

    mask64x2 &mask64x2::operator|=(mask64x2 other) noexcept
    {
        *this = *this | other;
        return *this;
    }

    mask64x2 &mask64x2::operator^=(mask64x2 other) noexcept
    {
        *this = *this ^ other;
        return *this;
    }

    bool any(mask32x4 mask) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return _mm_movemask_ps(to_m128_mask(mask)) != 0;
#elif CATALYST_MATH_HAS_NEON
        const uint32x4_t m = to_neon_mask32(mask);
        const std::uint32_t b0 = vgetq_lane_u32(m, 0) >> 31;
        const std::uint32_t b1 = vgetq_lane_u32(m, 1) >> 31;
        const std::uint32_t b2 = vgetq_lane_u32(m, 2) >> 31;
        const std::uint32_t b3 = vgetq_lane_u32(m, 3) >> 31;
        return (b0 | b1 | b2 | b3) != 0;
#else
        std::uint32_t bits[4]{};
        mask.store_unaligned(bits);
        const bool b0 = (bits[0] & 0x80000000u) != 0;
        const bool b1 = (bits[1] & 0x80000000u) != 0;
        const bool b2 = (bits[2] & 0x80000000u) != 0;
        const bool b3 = (bits[3] & 0x80000000u) != 0;
        return b0 || b1 || b2 || b3;
#endif
    }

    bool all(mask32x4 mask) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return _mm_movemask_ps(to_m128_mask(mask)) == 0xF;
#elif CATALYST_MATH_HAS_NEON
        const uint32x4_t m = to_neon_mask32(mask);
        const std::uint32_t b0 = vgetq_lane_u32(m, 0) >> 31;
        const std::uint32_t b1 = vgetq_lane_u32(m, 1) >> 31;
        const std::uint32_t b2 = vgetq_lane_u32(m, 2) >> 31;
        const std::uint32_t b3 = vgetq_lane_u32(m, 3) >> 31;
        return (b0 & b1 & b2 & b3) != 0;
#else
        std::uint32_t bits[4]{};
        mask.store_unaligned(bits);
        const bool b0 = (bits[0] & 0x80000000u) != 0;
        const bool b1 = (bits[1] & 0x80000000u) != 0;
        const bool b2 = (bits[2] & 0x80000000u) != 0;
        const bool b3 = (bits[3] & 0x80000000u) != 0;
        return b0 && b1 && b2 && b3;
#endif
    }

    bool any(mask64x2 mask) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return _mm_movemask_pd(to_m128d_mask(mask)) != 0;
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const uint64x2_t m = to_neon_mask64(mask);
        const std::uint64_t b0 = vgetq_lane_u64(m, 0) >> 63;
        const std::uint64_t b1 = vgetq_lane_u64(m, 1) >> 63;
        return (b0 | b1) != 0;
#else
        std::uint64_t bits[2]{};
        mask.store_unaligned(bits);
        const bool b0 = (bits[0] & 0x8000000000000000ull) != 0;
        const bool b1 = (bits[1] & 0x8000000000000000ull) != 0;
        return b0 || b1;
#endif
    }

    bool all(mask64x2 mask) noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        return _mm_movemask_pd(to_m128d_mask(mask)) == 0x3;
#elif CATALYST_MATH_HAS_NEON && defined(__aarch64__)
        const uint64x2_t m = to_neon_mask64(mask);
        const std::uint64_t b0 = vgetq_lane_u64(m, 0) >> 63;
        const std::uint64_t b1 = vgetq_lane_u64(m, 1) >> 63;
        return (b0 & b1) != 0;
#else
        std::uint64_t bits[2]{};
        mask.store_unaligned(bits);
        const bool b0 = (bits[0] & 0x8000000000000000ull) != 0;
        const bool b1 = (bits[1] & 0x8000000000000000ull) != 0;
        return b0 && b1;
#endif
    }

} // namespace catalyst::math
