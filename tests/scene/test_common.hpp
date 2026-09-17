#pragma once

#include <catalyst/math/matrix.hpp>
#include <catalyst/math/vector.hpp>

#include "../test_common.hpp"

#include <cmath>

namespace catalyst::tests
{

    inline bool near(float a, float b, float eps = 1e-4f) noexcept
    {
        return std::fabs(a - b) <= eps;
    }

    inline bool near_vec3(const catalyst::math::vec3f &v, float x, float y, float z, float eps = 1e-4f) noexcept
    {
        return near(v.x(), x, eps) && near(v.y(), y, eps) && near(v.z(), z, eps);
    }

    inline bool near_mat4(const catalyst::math::mat4f &a, const catalyst::math::mat4f &b, float eps = 1e-4f) noexcept
    {
        for (std::size_t i = 0; i < 4; ++i)
            for (std::size_t j = 0; j < 4; ++j)
                if (!near(a(i, j), b(i, j), eps))
                    return false;
        return true;
    }

} // namespace catalyst::tests
