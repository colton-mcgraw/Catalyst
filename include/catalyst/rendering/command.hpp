/**
 * SPDX-License-Identifier: CDDL-1.0
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Command lists: recorded sequences of GPU work (render passes, state binds, draws, dispatches, copies) that are
 * handed to the device with `submit`.
 * @details Recording is bracketed by `begin_recording` / `end_recording`. Draw calls must sit inside a
 * `begin_render_pass` / `end_render_pass` pair; dispatches and copies must sit outside one. Calls made in an invalid
 * state are ignored by release builds and reported by backends with validation enabled. A command list may be
 * re-submitted until it is re-recorded; re-recording resets it.
 *
 * **Binding model.** Shader resources are bound to numbered slots of four independent kinds - uniform buffers, storage
 * buffers, textures and samplers - and a block of up to `max_push_constant_bytes` inline constants. Textures and
 * samplers are separate objects (as in D3D12 and Metal); combine them in the shader. How a slot appears in shader code:
 *
 * | Call                  | Vulkan (SPIR-V)                | D3D12 (DXIL)      | Metal                 |
 * |-----------------------|--------------------------------|-------------------|-----------------------|
 * | `set_uniform_buffer`  | `layout(set = 0, binding = n)` | `register(b<n>)`  | `[[buffer(n)]]`       |
 * | `set_storage_buffer`  | `layout(set = 1, binding = n)` | `register(u<n>)`  | `[[buffer(16 + n)]]`  |
 * | `set_texture`         | `layout(set = 2, binding = n)` | `register(t<n>)`  | `[[texture(n)]]`      |
 * | `set_sampler`         | `layout(set = 3, binding = n)` | `register(s<n>)`  | `[[sampler(n)]]`      |
 * | `push_constants`      | `layout(push_constant)`        | root constants    | `[[buffer(30)]]`      |
 *
 * Bindings are visible to every shader stage and persist across `set_pipeline` calls within a recording. Slot indices
 * are bounded by `max_uniform_buffer_slots`, `max_storage_buffer_slots`, `max_texture_slots` and `max_sampler_slots`.
 *
 * **Coordinate conventions.** Clip space is +Y up with depth in [0, 1] on every backend (the Vulkan backend flips its
 * viewport to match); `viewport` and `scissor_rect` are in framebuffer pixels with the origin at the top-left.
 */

#pragma once

#include <catalyst/rendering/buffer.hpp>
#include <catalyst/rendering/device.hpp>
#include <catalyst/rendering/pipeline.hpp>
#include <catalyst/rendering/texture.hpp>
#include <catalyst/rendering/types.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace catalyst::rendering
{

    /**
     * @enum queue_type
     * @brief Which hardware queue a command list is recorded for.
     */
    enum class queue_type : std::uint8_t
    {
        graphics, ///< Accepts everything.
        compute,  ///< Dispatches and copies only.
        transfer, ///< Copies only.
    };

    /**
     * @struct command_list_desc
     * @brief Creation parameters for a command list.
     */
    struct command_list_desc
    {
        queue_type queue = queue_type::graphics;
        const char *debug_name = nullptr;
    };

    struct command_list_tag
    {
    };

    /**
     * @brief Handle to a command list. See `create_command_list`.
     */
    using command_list = resource_handle<command_list_tag>;

    // -----------------------------------------------------------------------------
    // Render passes
    // -----------------------------------------------------------------------------

    enum class load_op : std::uint8_t
    {
        load,      ///< Keep the attachment's previous contents.
        clear,     ///< Clear to the attachment's clear value.
        dont_care, ///< Contents are undefined; cheapest when everything will be overwritten.
    };

    enum class store_op : std::uint8_t
    {
        store,     ///< Keep the results after the pass.
        dont_care, ///< Results may be discarded (e.g. a depth buffer nobody reads back).
    };

    struct clear_color
    {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 1.0f;
    };

    struct color_attachment
    {
        texture target;
        load_op load = load_op::clear;
        store_op store = store_op::store;
        clear_color clear{};
    };

    struct depth_stencil_attachment
    {
        /** Leave invalid for passes without depth. */
        texture target;
        load_op load = load_op::clear;
        store_op store = store_op::dont_care;
        float clear_depth = 1.0f;
        std::uint8_t clear_stencil = 0;
    };

    /**
     * @struct render_pass_desc
     * @brief Attachments of a render pass. `color_attachments` is a non-owning view read only during
     * `begin_render_pass`.
     */
    struct render_pass_desc
    {
        std::span<const color_attachment> color_attachments;
        depth_stencil_attachment depth_stencil;
        const char *debug_name = nullptr;
    };

    // -----------------------------------------------------------------------------
    // Lifetime and recording state
    // -----------------------------------------------------------------------------

    [[nodiscard]] command_list create_command_list(const device &dev, const command_list_desc &desc = {});

    /**
     * @brief Releases the command list and resets `cl` to an invalid handle. Pending submissions must have completed.
     */
    void destroy_command_list(command_list &cl) noexcept;

    [[nodiscard]] bool is_valid(const command_list &cl) noexcept;

    /**
     * @brief Starts recording, discarding any previously recorded commands.
     * @return False if the list is invalid or already recording.
     */
    bool begin_recording(const command_list &cl);

    /**
     * @brief Finishes recording, making the list submittable.
     * @return False if the list is not recording or a render pass is still open.
     */
    bool end_recording(const command_list &cl);

    [[nodiscard]] bool is_recording(const command_list &cl) noexcept;

    // -----------------------------------------------------------------------------
    // Commands
    // -----------------------------------------------------------------------------

    void begin_render_pass(const command_list &cl, const render_pass_desc &desc) noexcept;
    void end_render_pass(const command_list &cl) noexcept;

    void set_pipeline(const command_list &cl, const pipeline &p) noexcept;
    void set_viewport(const command_list &cl, const viewport &vp) noexcept;
    void set_scissor(const command_list &cl, const scissor_rect &rect) noexcept;

    void set_vertex_buffer(const command_list &cl, std::uint32_t binding, const buffer &b,
                           std::size_t offset_bytes = 0) noexcept;
    void set_index_buffer(const command_list &cl, const buffer &b, index_type type,
                          std::size_t offset_bytes = 0) noexcept;

    /** @brief Binds a uniform (constant) buffer range to `slot`; `size_bytes == 0` means "to the end of the buffer". */
    void set_uniform_buffer(const command_list &cl, std::uint32_t slot, const buffer &b, std::size_t offset_bytes = 0,
                            std::size_t size_bytes = 0) noexcept;
    /** @brief Binds a storage (structured / UAV) buffer range to `slot`; `size_bytes == 0` means "to the end". */
    void set_storage_buffer(const command_list &cl, std::uint32_t slot, const buffer &b, std::size_t offset_bytes = 0,
                            std::size_t size_bytes = 0) noexcept;
    void set_texture(const command_list &cl, std::uint32_t slot, const texture &t) noexcept;
    void set_sampler(const command_list &cl, std::uint32_t slot, const sampler &s) noexcept;

    /** @brief Uploads up to `max_push_constant_bytes` of inline shader constants. */
    void push_constants(const command_list &cl, std::uint32_t offset_bytes, std::span<const std::byte> data) noexcept;

    void draw(const command_list &cl, std::uint32_t vertex_count, std::uint32_t instance_count = 1,
              std::uint32_t first_vertex = 0, std::uint32_t first_instance = 0) noexcept;
    void draw_indexed(const command_list &cl, std::uint32_t index_count, std::uint32_t instance_count = 1,
                      std::uint32_t first_index = 0, std::int32_t vertex_offset = 0,
                      std::uint32_t first_instance = 0) noexcept;

    void dispatch(const command_list &cl, std::uint32_t group_count_x, std::uint32_t group_count_y = 1,
                  std::uint32_t group_count_z = 1) noexcept;

    void copy_buffer(const command_list &cl, const buffer &src, std::size_t src_offset_bytes, const buffer &dst,
                     std::size_t dst_offset_bytes, std::size_t size_bytes) noexcept;

    // -----------------------------------------------------------------------------
    // Submission
    // -----------------------------------------------------------------------------

    /**
     * @brief Queues the recorded lists for execution in order. Ordering between successive `submit` calls is preserved.
     * @return False if any list is invalid or has not finished recording; nothing is submitted in that case.
     */
    bool submit(const device &dev, std::span<const command_list> lists);

    bool submit(const device &dev, const command_list &cl);

} // namespace catalyst::rendering
