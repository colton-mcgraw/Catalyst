/**
 * SPDX-License-Identifier: CDDL-1.0
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief CPU-side bookkeeping implementation of the rendering backend contract. It tracks resource lifetimes, enforces
 * the usage rules documented in the public headers and keeps buffer / shader contents in host memory, but performs no
 * GPU work. It is the resource layer of the "null" backend and also stands in for backends whose resource layer has not
 * been written yet, so the public API is exercisable (and testable) everywhere.
 */

#include "../detail_backend.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace catalyst::rendering::detail
{

    namespace
    {
        // ---------------------------------------------------------------------
        // Resource records
        // ---------------------------------------------------------------------

        struct device_state
        {
            device_desc desc;
            std::string application_name;
        };

        struct buffer_state
        {
            resource_id owner = 0;
            buffer_desc desc;
            std::string debug_name;
            std::vector<std::byte> data;
        };

        struct shader_state
        {
            resource_id owner = 0;
            shader_stage stage = shader_stage::vertex;
            shader_bytecode_format bytecode_format = shader_bytecode_format::spirv;
            std::string entry_point;
            std::string debug_name;
            std::vector<std::byte> bytecode;
        };

        struct texture_state
        {
            resource_id owner = 0;
            texture_desc desc;
            std::string debug_name;
            /** Non-zero when the texture is a back buffer owned by that swapchain. */
            resource_id swapchain = 0;
        };

        struct sampler_state
        {
            resource_id owner = 0;
            sampler_desc desc;
        };

        struct pipeline_state
        {
            resource_id owner = 0;
            pipeline_type type = pipeline_type::graphics;
        };

        struct swapchain_state
        {
            resource_id owner = 0;
            swapchain_desc desc;
            std::string debug_name;
            std::vector<resource_id> images;
            std::uint32_t next_image = 0;
            bool acquired = false;
        };

        struct pending_copy
        {
            resource_id src = 0;
            std::size_t src_offset = 0;
            resource_id dst = 0;
            std::size_t dst_offset = 0;
            std::size_t size = 0;
        };

        struct command_list_state
        {
            resource_id owner = 0;
            command_list_desc desc;
            bool recording = false;
            bool ready = false;
            bool in_render_pass = false;
            resource_id bound_pipeline = 0;
            std::size_t command_count = 0;
            std::vector<pending_copy> copies;
        };

        // ---------------------------------------------------------------------
        // Registry
        // ---------------------------------------------------------------------

        resource_id g_next_id = 1;

        std::unordered_map<resource_id, device_state> g_devices;
        std::unordered_map<resource_id, buffer_state> g_buffers;
        std::unordered_map<resource_id, shader_state> g_shaders;
        std::unordered_map<resource_id, texture_state> g_textures;
        std::unordered_map<resource_id, sampler_state> g_samplers;
        std::unordered_map<resource_id, pipeline_state> g_pipelines;
        std::unordered_map<resource_id, swapchain_state> g_swapchains;
        std::unordered_map<resource_id, command_list_state> g_command_lists;

        resource_id allocate_id() noexcept
        {
            return g_next_id++;
        }

        template <typename Map>
        typename Map::mapped_type *find(Map &map, resource_id id) noexcept
        {
            if (id == 0)
                return nullptr;
            auto it = map.find(id);
            return it == map.end() ? nullptr : &it->second;
        }

        template <typename Map>
        void erase_owned_by(Map &map, resource_id device)
        {
            std::erase_if(map, [device](const auto &entry) { return entry.second.owner == device; });
        }

        std::string copy_name(const char *name)
        {
            return name ? std::string{name} : std::string{};
        }

        const char *name_or_null(const std::string &name) noexcept
        {
            return name.empty() ? nullptr : name.c_str();
        }

        std::size_t texture_mip0_bytes(const texture_desc &desc) noexcept
        {
            return static_cast<std::size_t>(desc.extent.width) * desc.extent.height * desc.extent.depth *
                   format_size_bytes(desc.pixel_format);
        }

        void create_swapchain_images(resource_id id, swapchain_state &sc)
        {
            sc.images.clear();
            sc.images.reserve(sc.desc.image_count);
            for (std::uint32_t i = 0; i < sc.desc.image_count; ++i)
            {
                texture_state t;
                t.owner = sc.owner;
                t.swapchain = id;
                t.desc.dimension = texture_dimension::texture_2d;
                t.desc.extent = {sc.desc.extent.width, sc.desc.extent.height, 1};
                t.desc.pixel_format = sc.desc.pixel_format;
                t.desc.usage = texture_usage::render_target | texture_usage::transfer_src | texture_usage::transfer_dst;
                t.debug_name = sc.debug_name.empty() ? std::string{} : sc.debug_name + " image " + std::to_string(i);

                const resource_id image = allocate_id();
                g_textures.emplace(image, std::move(t));
                sc.images.push_back(image);
            }
            sc.next_image = 0;
            sc.acquired = false;
        }

        void destroy_swapchain_images(swapchain_state &sc) noexcept
        {
            for (const resource_id image : sc.images)
                g_textures.erase(image);
            sc.images.clear();
        }

        /** The command list if it exists and is currently recording; null otherwise. */
        command_list_state *recording_list(resource_id id) noexcept
        {
            command_list_state *cl = find(g_command_lists, id);
            return (cl && cl->recording) ? cl : nullptr;
        }

        void record(command_list_state &cl) noexcept
        {
            ++cl.command_count;
        }

        bool buffer_has_usage(resource_id id, buffer_usage usage) noexcept
        {
            const buffer_state *b = find(g_buffers, id);
            return b && has_flag(b->desc.usage, usage);
        }

        bool range_in_bounds(std::size_t size, std::size_t offset, std::size_t length) noexcept
        {
            return offset <= size && length <= size - offset;
        }
    } // namespace

    // -------------------------------------------------------------------------
    // Device
    // -------------------------------------------------------------------------

    resource_id create_device(const device_desc &desc)
    {
        device_state s;
        s.desc = desc;
        s.application_name = copy_name(desc.application_name);

        const resource_id id = allocate_id();
        g_devices.emplace(id, std::move(s));
        return id;
    }

    void destroy_device(resource_id id) noexcept
    {
        if (!find(g_devices, id))
            return;

        erase_owned_by(g_command_lists, id);
        erase_owned_by(g_swapchains, id);
        erase_owned_by(g_pipelines, id);
        erase_owned_by(g_samplers, id);
        erase_owned_by(g_textures, id);
        erase_owned_by(g_shaders, id);
        erase_owned_by(g_buffers, id);
        g_devices.erase(id);
    }

    bool is_device_valid(resource_id id) noexcept
    {
        return find(g_devices, id) != nullptr;
    }

    device_info get_device_info(resource_id id) noexcept
    {
        device_info info;
        if (!find(g_devices, id))
            return info;
        info.backend = backend_type();
        info.adapter_name = "Catalyst bookkeeping adapter (no GPU work)";
        info.dedicated_video_memory_bytes = 0;
        return info;
    }

    void wait_idle(resource_id /*id*/) noexcept
    {
    }

    // -------------------------------------------------------------------------
    // Buffers
    // -------------------------------------------------------------------------

    resource_id create_buffer(resource_id device, const buffer_desc &desc, std::span<const std::byte> initial_data)
    {
        if (!find(g_devices, device))
            return 0;

        buffer_state s;
        s.owner = device;
        s.desc = desc;
        s.debug_name = copy_name(desc.debug_name);
        s.data.resize(desc.size_bytes);
        if (!initial_data.empty())
            std::memcpy(s.data.data(), initial_data.data(), initial_data.size());

        const resource_id id = allocate_id();
        g_buffers.emplace(id, std::move(s));
        return id;
    }

    void destroy_buffer(resource_id id) noexcept
    {
        g_buffers.erase(id);
    }

    bool is_buffer_valid(resource_id id) noexcept
    {
        return find(g_buffers, id) != nullptr;
    }

    buffer_desc get_buffer_desc(resource_id id) noexcept
    {
        const buffer_state *b = find(g_buffers, id);
        if (!b)
            return {};
        buffer_desc d = b->desc;
        d.debug_name = name_or_null(b->debug_name);
        return d;
    }

    bool write_buffer(resource_id id, std::size_t offset, std::span<const std::byte> data) noexcept
    {
        buffer_state *b = find(g_buffers, id);
        if (!b || b->desc.access == memory_access::gpu_to_cpu)
            return false;
        if (!range_in_bounds(b->data.size(), offset, data.size()))
            return false;
        if (!data.empty())
            std::memcpy(b->data.data() + offset, data.data(), data.size());
        return true;
    }

    bool read_buffer(resource_id id, std::size_t offset, std::span<std::byte> out) noexcept
    {
        const buffer_state *b = find(g_buffers, id);
        if (!b || b->desc.access != memory_access::gpu_to_cpu)
            return false;
        if (!range_in_bounds(b->data.size(), offset, out.size()))
            return false;
        if (!out.empty())
            std::memcpy(out.data(), b->data.data() + offset, out.size());
        return true;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------

    resource_id create_shader(resource_id device, const shader_desc &desc)
    {
        if (!find(g_devices, device) || desc.bytecode.empty())
            return 0;

        shader_state s;
        s.owner = device;
        s.stage = desc.stage;
        s.bytecode_format = desc.bytecode_format;
        s.entry_point = copy_name(desc.entry_point);
        s.debug_name = copy_name(desc.debug_name);
        s.bytecode.assign(desc.bytecode.begin(), desc.bytecode.end());

        const resource_id id = allocate_id();
        g_shaders.emplace(id, std::move(s));
        return id;
    }

    void destroy_shader(resource_id id) noexcept
    {
        g_shaders.erase(id);
    }

    bool is_shader_valid(resource_id id) noexcept
    {
        return find(g_shaders, id) != nullptr;
    }

    std::span<const std::byte> get_bytecode(resource_id id) noexcept
    {
        const shader_state *s = find(g_shaders, id);
        if (!s)
            return {};
        return s->bytecode;
    }

    shader_stage get_shader_stage(resource_id id) noexcept
    {
        const shader_state *s = find(g_shaders, id);
        return s ? s->stage : shader_stage::vertex;
    }

    // -------------------------------------------------------------------------
    // Textures and samplers
    // -------------------------------------------------------------------------

    resource_id create_texture(resource_id device, const texture_desc &desc, std::span<const std::byte> initial_data)
    {
        if (!find(g_devices, device))
            return 0;
        if (!initial_data.empty() && initial_data.size() != texture_mip0_bytes(desc))
            return 0;

        texture_state s;
        s.owner = device;
        s.desc = desc;
        s.debug_name = copy_name(desc.debug_name);

        const resource_id id = allocate_id();
        g_textures.emplace(id, std::move(s));
        return id;
    }

    void destroy_texture(resource_id id) noexcept
    {
        const texture_state *t = find(g_textures, id);
        if (!t || t->swapchain != 0)
            return; // Swapchain images are owned by their swapchain.
        g_textures.erase(id);
    }

    bool is_texture_valid(resource_id id) noexcept
    {
        return find(g_textures, id) != nullptr;
    }

    texture_desc get_texture_desc(resource_id id) noexcept
    {
        const texture_state *t = find(g_textures, id);
        if (!t)
            return {};
        texture_desc d = t->desc;
        d.debug_name = name_or_null(t->debug_name);
        return d;
    }

    resource_id create_sampler(resource_id device, const sampler_desc &desc)
    {
        if (!find(g_devices, device))
            return 0;

        sampler_state s;
        s.owner = device;
        s.desc = desc;
        s.desc.debug_name = nullptr;

        const resource_id id = allocate_id();
        g_samplers.emplace(id, s);
        return id;
    }

    void destroy_sampler(resource_id id) noexcept
    {
        g_samplers.erase(id);
    }

    bool is_sampler_valid(resource_id id) noexcept
    {
        return find(g_samplers, id) != nullptr;
    }

    // -------------------------------------------------------------------------
    // Pipelines
    // -------------------------------------------------------------------------

    resource_id create_graphics_pipeline(resource_id device, const graphics_pipeline_desc &desc)
    {
        if (!find(g_devices, device))
            return 0;

        const shader_state *vs = find(g_shaders, desc.vertex_shader.id());
        if (!vs || vs->owner != device || vs->stage != shader_stage::vertex)
            return 0;

        if (desc.fragment_shader)
        {
            const shader_state *fs = find(g_shaders, desc.fragment_shader.id());
            if (!fs || fs->owner != device || fs->stage != shader_stage::fragment)
                return 0;
        }

        pipeline_state s;
        s.owner = device;
        s.type = pipeline_type::graphics;

        const resource_id id = allocate_id();
        g_pipelines.emplace(id, s);
        return id;
    }

    resource_id create_compute_pipeline(resource_id device, const compute_pipeline_desc &desc)
    {
        if (!find(g_devices, device))
            return 0;

        const shader_state *cs = find(g_shaders, desc.compute_shader.id());
        if (!cs || cs->owner != device || cs->stage != shader_stage::compute)
            return 0;

        pipeline_state s;
        s.owner = device;
        s.type = pipeline_type::compute;

        const resource_id id = allocate_id();
        g_pipelines.emplace(id, s);
        return id;
    }

    void destroy_pipeline(resource_id id) noexcept
    {
        g_pipelines.erase(id);
    }

    bool is_pipeline_valid(resource_id id) noexcept
    {
        return find(g_pipelines, id) != nullptr;
    }

    pipeline_type get_pipeline_type(resource_id id) noexcept
    {
        const pipeline_state *p = find(g_pipelines, id);
        return p ? p->type : pipeline_type::graphics;
    }

    // -------------------------------------------------------------------------
    // Swapchains
    // -------------------------------------------------------------------------

    resource_id create_swapchain(resource_id device, const swapchain_desc &desc)
    {
        if (!find(g_devices, device))
            return 0;

        swapchain_state s;
        s.owner = device;
        s.desc = desc;
        s.debug_name = copy_name(desc.debug_name);

        const resource_id id = allocate_id();
        auto [it, inserted] = g_swapchains.emplace(id, std::move(s));
        create_swapchain_images(id, it->second);
        return id;
    }

    void destroy_swapchain(resource_id id) noexcept
    {
        swapchain_state *sc = find(g_swapchains, id);
        if (!sc)
            return;
        destroy_swapchain_images(*sc);
        g_swapchains.erase(id);
    }

    bool is_swapchain_valid(resource_id id) noexcept
    {
        return find(g_swapchains, id) != nullptr;
    }

    swapchain_desc get_swapchain_desc(resource_id id) noexcept
    {
        const swapchain_state *sc = find(g_swapchains, id);
        if (!sc)
            return {};
        swapchain_desc d = sc->desc;
        d.debug_name = name_or_null(sc->debug_name);
        return d;
    }

    bool resize_swapchain(resource_id id, extent2d extent)
    {
        swapchain_state *sc = find(g_swapchains, id);
        if (!sc)
            return false;
        destroy_swapchain_images(*sc);
        sc->desc.extent = extent;
        create_swapchain_images(id, *sc);
        return true;
    }

    resource_id acquire_next_image(resource_id id)
    {
        swapchain_state *sc = find(g_swapchains, id);
        if (!sc || sc->images.empty())
            return 0;
        const resource_id image = sc->images[sc->next_image];
        sc->next_image = (sc->next_image + 1) % static_cast<std::uint32_t>(sc->images.size());
        sc->acquired = true;
        return image;
    }

    bool present(resource_id id)
    {
        swapchain_state *sc = find(g_swapchains, id);
        if (!sc || !sc->acquired)
            return false;
        sc->acquired = false;
        return true;
    }

    // -------------------------------------------------------------------------
    // Command lists
    // -------------------------------------------------------------------------

    resource_id create_command_list(resource_id device, const command_list_desc &desc)
    {
        if (!find(g_devices, device))
            return 0;

        command_list_state s;
        s.owner = device;
        s.desc = desc;
        s.desc.debug_name = nullptr;

        const resource_id id = allocate_id();
        g_command_lists.emplace(id, std::move(s));
        return id;
    }

    void destroy_command_list(resource_id id) noexcept
    {
        g_command_lists.erase(id);
    }

    bool is_command_list_valid(resource_id id) noexcept
    {
        return find(g_command_lists, id) != nullptr;
    }

    bool begin_recording(resource_id id)
    {
        command_list_state *cl = find(g_command_lists, id);
        if (!cl || cl->recording)
            return false;
        cl->recording = true;
        cl->ready = false;
        cl->in_render_pass = false;
        cl->bound_pipeline = 0;
        cl->command_count = 0;
        cl->copies.clear();
        return true;
    }

    bool end_recording(resource_id id)
    {
        command_list_state *cl = recording_list(id);
        if (!cl || cl->in_render_pass)
            return false;
        cl->recording = false;
        cl->ready = true;
        return true;
    }

    bool is_recording(resource_id id) noexcept
    {
        return recording_list(id) != nullptr;
    }

    void begin_render_pass(resource_id id, const render_pass_desc &desc) noexcept
    {
        command_list_state *cl = recording_list(id);
        if (!cl || cl->in_render_pass || cl->desc.queue != queue_type::graphics)
            return;
        if (desc.color_attachments.size() > max_color_attachments)
            return;
        if (desc.color_attachments.empty() && !desc.depth_stencil.target)
            return;

        for (const color_attachment &a : desc.color_attachments)
        {
            const texture_state *t = find(g_textures, a.target.id());
            if (!t || !has_flag(t->desc.usage, texture_usage::render_target))
                return;
        }

        if (desc.depth_stencil.target)
        {
            const texture_state *t = find(g_textures, desc.depth_stencil.target.id());
            if (!t || !has_flag(t->desc.usage, texture_usage::depth_stencil))
                return;
        }

        cl->in_render_pass = true;
        record(*cl);
    }

    void end_render_pass(resource_id id) noexcept
    {
        command_list_state *cl = recording_list(id);
        if (!cl || !cl->in_render_pass)
            return;
        cl->in_render_pass = false;
        record(*cl);
    }

    void set_pipeline(resource_id id, resource_id pipeline) noexcept
    {
        command_list_state *cl = recording_list(id);
        if (!cl || !find(g_pipelines, pipeline))
            return;
        cl->bound_pipeline = pipeline;
        record(*cl);
    }

    void set_viewport(resource_id id, const viewport & /*vp*/) noexcept
    {
        if (command_list_state *cl = recording_list(id))
            record(*cl);
    }

    void set_scissor(resource_id id, const scissor_rect & /*rect*/) noexcept
    {
        if (command_list_state *cl = recording_list(id))
            record(*cl);
    }

    void set_vertex_buffer(resource_id id, std::uint32_t binding, resource_id buffer, std::size_t /*offset*/) noexcept
    {
        command_list_state *cl = recording_list(id);
        if (!cl || binding >= max_vertex_bindings || !buffer_has_usage(buffer, buffer_usage::vertex))
            return;
        record(*cl);
    }

    void set_index_buffer(resource_id id, resource_id buffer, index_type /*type*/, std::size_t /*offset*/) noexcept
    {
        command_list_state *cl = recording_list(id);
        if (!cl || !buffer_has_usage(buffer, buffer_usage::index))
            return;
        record(*cl);
    }

    void set_uniform_buffer(resource_id id, std::uint32_t /*slot*/, resource_id buffer, std::size_t /*offset*/,
                            std::size_t /*size*/) noexcept
    {
        command_list_state *cl = recording_list(id);
        if (!cl || !buffer_has_usage(buffer, buffer_usage::uniform))
            return;
        record(*cl);
    }

    void set_storage_buffer(resource_id id, std::uint32_t /*slot*/, resource_id buffer, std::size_t /*offset*/,
                            std::size_t /*size*/) noexcept
    {
        command_list_state *cl = recording_list(id);
        if (!cl || !buffer_has_usage(buffer, buffer_usage::storage))
            return;
        record(*cl);
    }

    void set_texture(resource_id id, std::uint32_t /*slot*/, resource_id texture) noexcept
    {
        command_list_state *cl = recording_list(id);
        if (!cl || !find(g_textures, texture))
            return;
        record(*cl);
    }

    void set_sampler(resource_id id, std::uint32_t /*slot*/, resource_id sampler) noexcept
    {
        command_list_state *cl = recording_list(id);
        if (!cl || !find(g_samplers, sampler))
            return;
        record(*cl);
    }

    void push_constants(resource_id id, std::uint32_t /*offset*/, std::span<const std::byte> /*data*/) noexcept
    {
        if (command_list_state *cl = recording_list(id))
            record(*cl);
    }

    void draw(resource_id id, std::uint32_t /*vertex_count*/, std::uint32_t /*instance_count*/,
              std::uint32_t /*first_vertex*/, std::uint32_t /*first_instance*/) noexcept
    {
        command_list_state *cl = recording_list(id);
        if (!cl || !cl->in_render_pass)
            return;
        const pipeline_state *p = find(g_pipelines, cl->bound_pipeline);
        if (!p || p->type != pipeline_type::graphics)
            return;
        record(*cl);
    }

    void draw_indexed(resource_id id, std::uint32_t /*index_count*/, std::uint32_t /*instance_count*/,
                      std::uint32_t /*first_index*/, std::int32_t /*vertex_offset*/,
                      std::uint32_t /*first_instance*/) noexcept
    {
        command_list_state *cl = recording_list(id);
        if (!cl || !cl->in_render_pass)
            return;
        const pipeline_state *p = find(g_pipelines, cl->bound_pipeline);
        if (!p || p->type != pipeline_type::graphics)
            return;
        record(*cl);
    }

    void dispatch(resource_id id, std::uint32_t /*x*/, std::uint32_t /*y*/, std::uint32_t /*z*/) noexcept
    {
        command_list_state *cl = recording_list(id);
        if (!cl || cl->in_render_pass || cl->desc.queue == queue_type::transfer)
            return;
        const pipeline_state *p = find(g_pipelines, cl->bound_pipeline);
        if (!p || p->type != pipeline_type::compute)
            return;
        record(*cl);
    }

    void copy_buffer(resource_id id, resource_id src, std::size_t src_offset, resource_id dst, std::size_t dst_offset,
                     std::size_t size) noexcept
    {
        command_list_state *cl = recording_list(id);
        if (!cl || cl->in_render_pass)
            return;

        const buffer_state *s = find(g_buffers, src);
        const buffer_state *d = find(g_buffers, dst);
        if (!s || !d)
            return;
        if (!range_in_bounds(s->data.size(), src_offset, size) || !range_in_bounds(d->data.size(), dst_offset, size))
            return;

        cl->copies.push_back({src, src_offset, dst, dst_offset, size});
        record(*cl);
    }

    bool submit(resource_id device, std::span<const command_list> lists)
    {
        if (!find(g_devices, device))
            return false;

        for (const command_list &handle : lists)
        {
            const command_list_state *cl = find(g_command_lists, handle.id());
            if (!cl || cl->owner != device || !cl->ready || cl->recording)
                return false;
        }

        // "Execute": the only observable work the bookkeeping backend performs is buffer-to-buffer copies.
        for (const command_list &handle : lists)
        {
            const command_list_state *cl = find(g_command_lists, handle.id());
            for (const pending_copy &c : cl->copies)
            {
                const buffer_state *s = find(g_buffers, c.src);
                buffer_state *d = find(g_buffers, c.dst);
                if (!s || !d)
                    continue;
                if (!range_in_bounds(s->data.size(), c.src_offset, c.size) ||
                    !range_in_bounds(d->data.size(), c.dst_offset, c.size))
                    continue;
                std::memmove(d->data.data() + c.dst_offset, s->data.data() + c.src_offset, c.size);
            }
        }

        return true;
    }

} // namespace catalyst::rendering::detail
