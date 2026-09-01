/**
 * SPDX-License-Identifier: CDDL-1.0
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Public shader API; validates handles and arguments, then forwards to the active backend.
 */

#include <catalyst/rendering/shader.hpp>

#include "detail_backend.hpp"

namespace catalyst::rendering
{

    shader create_shader(const device &dev, const shader_desc &desc)
    {
        if (!dev || desc.bytecode.empty())
            return {};
        return shader{detail::create_shader(dev.id(), desc)};
    }

    void destroy_shader(shader &s) noexcept
    {
        if (!s)
            return;
        detail::destroy_shader(s.id());
        s = shader{};
    }

    bool is_valid(const shader &s) noexcept
    {
        return s && detail::is_shader_valid(s.id());
    }

    std::span<const std::byte> get_bytecode(const shader &s) noexcept
    {
        if (!s)
            return {};
        return detail::get_bytecode(s.id());
    }

    shader_stage get_shader_stage(const shader &s) noexcept
    {
        if (!s)
            return shader_stage::vertex;
        return detail::get_shader_stage(s.id());
    }

} // namespace catalyst::rendering
