/**
 * SPDX-License-Identifier: CDDL-1.0
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Internal contract every rendering backend implements. The public API in include/catalyst/rendering validates
 * handles and forwards to these functions with raw `resource_id`s; backends never see public handle types except
 * inside descriptor structs. Not part of the public API.
 */

#pragma once

#include <catalyst/rendering/rendering.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace catalyst::rendering::detail
{

    // -------------------------------------------------------------------------
    // Identity
    // -------------------------------------------------------------------------

    const char *backend_name();
    backend_kind backend_type() noexcept;

    // -------------------------------------------------------------------------
    // Device
    // -------------------------------------------------------------------------

    resource_id create_device(const device_desc &desc);
    void destroy_device(resource_id id) noexcept;
    bool is_device_valid(resource_id id) noexcept;
    device_info get_device_info(resource_id id) noexcept;
    void wait_idle(resource_id id) noexcept;

    // -------------------------------------------------------------------------
    // Buffers
    // -------------------------------------------------------------------------

    resource_id create_buffer(resource_id device, const buffer_desc &desc, std::span<const std::byte> initial_data);
    void destroy_buffer(resource_id id) noexcept;
    bool is_buffer_valid(resource_id id) noexcept;
    buffer_desc get_buffer_desc(resource_id id) noexcept;
    bool write_buffer(resource_id id, std::size_t offset, std::span<const std::byte> data) noexcept;
    bool read_buffer(resource_id id, std::size_t offset, std::span<std::byte> out) noexcept;

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------

    resource_id create_shader(resource_id device, const shader_desc &desc);
    void destroy_shader(resource_id id) noexcept;
    bool is_shader_valid(resource_id id) noexcept;
    std::span<const std::byte> get_bytecode(resource_id id) noexcept;
    shader_stage get_shader_stage(resource_id id) noexcept;

    // -------------------------------------------------------------------------
    // Textures and samplers
    // -------------------------------------------------------------------------

    resource_id create_texture(resource_id device, const texture_desc &desc, std::span<const std::byte> initial_data);
    void destroy_texture(resource_id id) noexcept;
    bool is_texture_valid(resource_id id) noexcept;
    texture_desc get_texture_desc(resource_id id) noexcept;

    resource_id create_sampler(resource_id device, const sampler_desc &desc);
    void destroy_sampler(resource_id id) noexcept;
    bool is_sampler_valid(resource_id id) noexcept;

    // -------------------------------------------------------------------------
    // Pipelines
    // -------------------------------------------------------------------------

    resource_id create_graphics_pipeline(resource_id device, const graphics_pipeline_desc &desc);
    resource_id create_compute_pipeline(resource_id device, const compute_pipeline_desc &desc);
    void destroy_pipeline(resource_id id) noexcept;
    bool is_pipeline_valid(resource_id id) noexcept;
    pipeline_type get_pipeline_type(resource_id id) noexcept;

    // -------------------------------------------------------------------------
    // Swapchains
    // -------------------------------------------------------------------------

    resource_id create_swapchain(resource_id device, const swapchain_desc &desc);
    void destroy_swapchain(resource_id id) noexcept;
    bool is_swapchain_valid(resource_id id) noexcept;
    swapchain_desc get_swapchain_desc(resource_id id) noexcept;
    bool resize_swapchain(resource_id id, extent2d extent);
    resource_id acquire_next_image(resource_id id);
    bool present(resource_id id);

    // -------------------------------------------------------------------------
    // Command lists
    // -------------------------------------------------------------------------

    resource_id create_command_list(resource_id device, const command_list_desc &desc);
    void destroy_command_list(resource_id id) noexcept;
    bool is_command_list_valid(resource_id id) noexcept;
    bool begin_recording(resource_id id);
    bool end_recording(resource_id id);
    bool is_recording(resource_id id) noexcept;

    void begin_render_pass(resource_id id, const render_pass_desc &desc) noexcept;
    void end_render_pass(resource_id id) noexcept;
    void set_pipeline(resource_id id, resource_id pipeline) noexcept;
    void set_viewport(resource_id id, const viewport &vp) noexcept;
    void set_scissor(resource_id id, const scissor_rect &rect) noexcept;
    void set_vertex_buffer(resource_id id, std::uint32_t binding, resource_id buffer, std::size_t offset) noexcept;
    void set_index_buffer(resource_id id, resource_id buffer, index_type type, std::size_t offset) noexcept;
    void set_uniform_buffer(resource_id id, std::uint32_t slot, resource_id buffer, std::size_t offset,
                            std::size_t size) noexcept;
    void set_storage_buffer(resource_id id, std::uint32_t slot, resource_id buffer, std::size_t offset,
                            std::size_t size) noexcept;
    void set_texture(resource_id id, std::uint32_t slot, resource_id texture) noexcept;
    void set_sampler(resource_id id, std::uint32_t slot, resource_id sampler) noexcept;
    void push_constants(resource_id id, std::uint32_t offset, std::span<const std::byte> data) noexcept;
    void draw(resource_id id, std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex,
              std::uint32_t first_instance) noexcept;
    void draw_indexed(resource_id id, std::uint32_t index_count, std::uint32_t instance_count,
                      std::uint32_t first_index, std::int32_t vertex_offset, std::uint32_t first_instance) noexcept;
    void dispatch(resource_id id, std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept;
    void copy_buffer(resource_id id, resource_id src, std::size_t src_offset, resource_id dst, std::size_t dst_offset,
                     std::size_t size) noexcept;

    bool submit(resource_id device, std::span<const command_list> lists);

} // namespace catalyst::rendering::detail
