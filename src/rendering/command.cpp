/**
 * SPDX-License-Identifier: CDDL-1.0
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Public command-list API; validates handles and forwards to the active backend.
 */

#include <catalyst/rendering/command.hpp>

#include "detail_backend.hpp"

namespace catalyst::rendering
{

    command_list create_command_list(const device &dev, const command_list_desc &desc)
    {
        if (!dev)
            return {};
        return command_list{detail::create_command_list(dev.id(), desc)};
    }

    void destroy_command_list(command_list &cl) noexcept
    {
        if (!cl)
            return;
        detail::destroy_command_list(cl.id());
        cl = command_list{};
    }

    bool is_valid(const command_list &cl) noexcept
    {
        return cl && detail::is_command_list_valid(cl.id());
    }

    bool begin_recording(const command_list &cl)
    {
        return cl && detail::begin_recording(cl.id());
    }

    bool end_recording(const command_list &cl)
    {
        return cl && detail::end_recording(cl.id());
    }

    bool is_recording(const command_list &cl) noexcept
    {
        return cl && detail::is_recording(cl.id());
    }

    void begin_render_pass(const command_list &cl, const render_pass_desc &desc) noexcept
    {
        if (!cl)
            return;
        detail::begin_render_pass(cl.id(), desc);
    }

    void end_render_pass(const command_list &cl) noexcept
    {
        if (!cl)
            return;
        detail::end_render_pass(cl.id());
    }

    void set_pipeline(const command_list &cl, const pipeline &p) noexcept
    {
        if (!cl || !p)
            return;
        detail::set_pipeline(cl.id(), p.id());
    }

    void set_viewport(const command_list &cl, const viewport &vp) noexcept
    {
        if (!cl)
            return;
        detail::set_viewport(cl.id(), vp);
    }

    void set_scissor(const command_list &cl, const scissor_rect &rect) noexcept
    {
        if (!cl)
            return;
        detail::set_scissor(cl.id(), rect);
    }

    void set_vertex_buffer(const command_list &cl, std::uint32_t binding, const buffer &b,
                           std::size_t offset_bytes) noexcept
    {
        if (!cl || !b)
            return;
        detail::set_vertex_buffer(cl.id(), binding, b.id(), offset_bytes);
    }

    void set_index_buffer(const command_list &cl, const buffer &b, index_type type, std::size_t offset_bytes) noexcept
    {
        if (!cl || !b)
            return;
        detail::set_index_buffer(cl.id(), b.id(), type, offset_bytes);
    }

    void set_uniform_buffer(const command_list &cl, std::uint32_t slot, const buffer &b, std::size_t offset_bytes,
                            std::size_t size_bytes) noexcept
    {
        if (!cl || !b)
            return;
        detail::set_uniform_buffer(cl.id(), slot, b.id(), offset_bytes, size_bytes);
    }

    void set_storage_buffer(const command_list &cl, std::uint32_t slot, const buffer &b, std::size_t offset_bytes,
                            std::size_t size_bytes) noexcept
    {
        if (!cl || !b)
            return;
        detail::set_storage_buffer(cl.id(), slot, b.id(), offset_bytes, size_bytes);
    }

    void set_texture(const command_list &cl, std::uint32_t slot, const texture &t) noexcept
    {
        if (!cl || !t)
            return;
        detail::set_texture(cl.id(), slot, t.id());
    }

    void set_sampler(const command_list &cl, std::uint32_t slot, const sampler &s) noexcept
    {
        if (!cl || !s)
            return;
        detail::set_sampler(cl.id(), slot, s.id());
    }

    void push_constants(const command_list &cl, std::uint32_t offset_bytes, std::span<const std::byte> data) noexcept
    {
        if (!cl || data.empty())
            return;
        if (offset_bytes > max_push_constant_bytes || data.size() > max_push_constant_bytes - offset_bytes)
            return;
        detail::push_constants(cl.id(), offset_bytes, data);
    }

    void draw(const command_list &cl, std::uint32_t vertex_count, std::uint32_t instance_count,
              std::uint32_t first_vertex, std::uint32_t first_instance) noexcept
    {
        if (!cl || vertex_count == 0 || instance_count == 0)
            return;
        detail::draw(cl.id(), vertex_count, instance_count, first_vertex, first_instance);
    }

    void draw_indexed(const command_list &cl, std::uint32_t index_count, std::uint32_t instance_count,
                      std::uint32_t first_index, std::int32_t vertex_offset, std::uint32_t first_instance) noexcept
    {
        if (!cl || index_count == 0 || instance_count == 0)
            return;
        detail::draw_indexed(cl.id(), index_count, instance_count, first_index, vertex_offset, first_instance);
    }

    void dispatch(const command_list &cl, std::uint32_t group_count_x, std::uint32_t group_count_y,
                  std::uint32_t group_count_z) noexcept
    {
        if (!cl || group_count_x == 0 || group_count_y == 0 || group_count_z == 0)
            return;
        detail::dispatch(cl.id(), group_count_x, group_count_y, group_count_z);
    }

    void copy_buffer(const command_list &cl, const buffer &src, std::size_t src_offset_bytes, const buffer &dst,
                     std::size_t dst_offset_bytes, std::size_t size_bytes) noexcept
    {
        if (!cl || !src || !dst || size_bytes == 0)
            return;
        detail::copy_buffer(cl.id(), src.id(), src_offset_bytes, dst.id(), dst_offset_bytes, size_bytes);
    }

    bool submit(const device &dev, std::span<const command_list> lists)
    {
        if (!dev || lists.empty())
            return false;
        for (const command_list &cl : lists)
        {
            if (!cl)
                return false;
        }
        return detail::submit(dev.id(), lists);
    }

    bool submit(const device &dev, const command_list &cl)
    {
        return submit(dev, std::span<const command_list>{&cl, 1});
    }

} // namespace catalyst::rendering
