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

    f32x4 f32x4::mask() const noexcept
    {
#if CATALYST_MATH_HAS_SSE2
        const __m128 x = to_m128(*this);
        const __m128 z = _mm_setzero_ps();
        return from_m128(_mm_cmplt_ps(x, z));
#elif CATALYST_MATH_HAS_NEON
        const float32x4_t x = to_neon(*this);
        const float32x4_t z = vdupq_n_f32(0.0f);
        // vcltq_f32 returns uint32x4_t; reinterpret as float bits.
        const uint32x4_t m = vcltq_f32(x, z);
        return from_neon(vreinterpretq_f32_u32(m));
#else
        float f[4];
        store_scalar(f, *this);
        const std::uint32_t m0 = (f[0] < 0.0f) ? 0xFFFFFFFFu : 0u;
        const std::uint32_t m1 = (f[1] < 0.0f) ? 0xFFFFFFFFu : 0u;
        const std::uint32_t m2 = (f[2] < 0.0f) ? 0xFFFFFFFFu : 0u;
        const std::uint32_t m3 = (f[3] < 0.0f) ? 0xFFFFFFFFu : 0u;
        const std::uint32_t out_u32[4] = {m0, m1, m2, m3};
        f32x4 out;
        std::memcpy(detail::f32x4_access::data(out), out_u32, 16);
        return out;
#endif
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

} // namespace catalyst::math
