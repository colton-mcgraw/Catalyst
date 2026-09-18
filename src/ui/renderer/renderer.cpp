/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Implementation of the renderer bridge: one pipeline, per-frame buffers and layer images,
 * a texture registry, one draw per command.
 */

#include <catalyst/rendering/shader.hpp>
#include <catalyst/ui/renderer.hpp>

#include "shaders.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace catalyst::ui
{
    namespace
    {
        /**
         * @brief What the shaders' push-constant block holds.
         * @details `clip = pixel * scale + translate`. The Vulkan backend flips its viewport so clip-space y
         * points up, as GL's does, which is why the y scale is negative and the y translate is +1 for the
         * top-left origin: pixel row 0 lands at the top. `mode` and `opacity` are the fragment stage's:
         * see shaders/ui.frag for the modes.
         */
        struct push_block
        {
            float scale[2];
            float translate[2];
            std::uint32_t mode;
            float opacity;
        };
        static_assert(sizeof(push_block) == 24);

        /** @brief Byte offset of `push_block::mode`, the start of the part that changes per draw. */
        constexpr std::uint32_t k_mode_offset = 16;

        constexpr std::uint32_t k_mode_solid = 0;
        constexpr std::uint32_t k_mode_mask = 1;
        constexpr std::uint32_t k_mode_color = 2;
        constexpr std::uint32_t k_mode_layer = 3;

        // The vertex layout below spells the struct out by offset; keep it honest.
        static_assert(sizeof(vertex) == 20, "ui::vertex changed size; update the vertex layout in renderer.cpp");
        static_assert(sizeof(point) == 8, "ui::point changed size; update the vertex layout in renderer.cpp");
        static_assert(sizeof(index) == 4, "ui::index changed size; update the index type in renderer.cpp");

        constexpr std::uint32_t k_position_offset = 0;
        constexpr std::uint32_t k_uv_offset = 8;
        constexpr std::uint32_t k_color_offset = 16;

        /** @brief The texture and sampler slot the shaders read: set 2 / set 3, binding 0. */
        constexpr std::uint32_t k_texture_slot = 0;

        /** @brief Vertices and indices one layer's composite quad takes at the end of the slot's buffers. */
        constexpr std::uint32_t k_quad_vertices = 4;
        constexpr std::uint32_t k_quad_indices = 6;

        /**
         * @brief Premultiplied blending: the fragment stage multiplies colour by alpha itself.
         * @details Draws identically to `blend_alpha` for ordinary geometry, and is what makes a layer
         * image, which holds premultiplied colour after being drawn into from transparent, composite
         * correctly when sampled back.
         */
        constexpr rendering::blend_state blend_premultiplied() noexcept
        {
            rendering::blend_state s;
            s.enabled = true;
            s.src_color = rendering::blend_factor::one;
            s.dst_color = rendering::blend_factor::one_minus_src_alpha;
            s.src_alpha = rendering::blend_factor::one;
            s.dst_alpha = rendering::blend_factor::one_minus_src_alpha;
            return s;
        }

        /**
         * @brief Replaces `out` with a fresh host-visible buffer of `count` elements, destroying the old one.
         * @details Host-visible because the whole buffer is rewritten every frame; a device-local copy
         * would cost a staging pass for no gain on geometry the GPU reads once.
         */
        bool replace_buffer(const rendering::device &dev, rendering::buffer_usage usage, std::uint32_t stride,
                            std::uint32_t count, const char *name, rendering::buffer &out)
        {
            rendering::buffer_desc desc;
            desc.size_bytes = static_cast<std::size_t>(count) * stride;
            desc.usage = usage;
            desc.access = rendering::memory_access::cpu_to_gpu;
            desc.stride_bytes = stride;
            desc.debug_name = name;

            rendering::buffer fresh = rendering::create_buffer(dev, desc);
            if (!fresh)
                return false;

            rendering::destroy_buffer(out);
            out = fresh;
            return true;
        }

        /**
         * @brief Clamps a rectangle, shifted so that `(origin_x, origin_y)` is the target's top-left, to the
         * target and converts it to a scissor.
         * @return False when nothing of the rectangle lies inside the target.
         */
        bool scissor_of(const rect &r, rendering::extent2d target, std::int32_t origin_x, std::int32_t origin_y,
                        rendering::scissor_rect &out) noexcept
        {
            const float w = static_cast<float>(target.width);
            const float h = static_cast<float>(target.height);
            const float ox = static_cast<float>(origin_x);
            const float oy = static_cast<float>(origin_y);

            // Clamp in float first: the builder's unbounded clip is +/- infinity, which no integer holds.
            const float x0 = std::floor(std::clamp(r.min.x() - ox, 0.0f, w));
            const float y0 = std::floor(std::clamp(r.min.y() - oy, 0.0f, h));
            const float x1 = std::ceil(std::clamp(r.max.x() - ox, 0.0f, w));
            const float y1 = std::ceil(std::clamp(r.max.y() - oy, 0.0f, h));
            if (!(x1 > x0) || !(y1 > y0))
                return false;

            out.x = static_cast<std::int32_t>(x0);
            out.y = static_cast<std::int32_t>(y0);
            out.width = static_cast<std::uint32_t>(x1 - x0);
            out.height = static_cast<std::uint32_t>(y1 - y0);
            return true;
        }

        /**
         * @brief Checks the invariants `draw_range` relies on so a hand-built batch cannot index out of range.
         * @details Every command's layer exists; every layer's parent is an earlier layer; a layer's
         * command range lies within the batch and within its parent's range.
         */
        bool layers_well_formed(const render_batch &batch) noexcept
        {
            const std::size_t layer_count = batch.layers.size();
            const std::size_t command_count = batch.commands.size();

            for (const draw_command &c : batch.commands)
                if (c.layer != no_layer && c.layer >= layer_count)
                    return false;

            for (std::size_t i = 0; i < layer_count; ++i)
            {
                const layer &l = batch.layers[i];
                if (l.first_command > l.end_command || l.end_command > command_count)
                    return false;
                if (l.parent == no_layer)
                    continue;
                if (l.parent >= i)
                    return false;
                const layer &p = batch.layers[l.parent];
                if (l.first_command < p.first_command || l.end_command > p.end_command)
                    return false;
            }
            return true;
        }

        /** @brief True when a layer has commands to draw and would show at all. */
        bool layer_drawable(const layer &l) noexcept
        {
            return l.end_command > l.first_command && l.opacity > 0.0f && !l.bounds.is_empty();
        }
    } // namespace

    // ---- draw state -----------------------------------------------------------------------------

    /**
     * @brief The bindings in effect while a range of commands is recorded.
     * @details Texture and mode changes are recorded only when they differ from what is bound, so a
     * run of solid commands costs nothing between draws.
     */
    struct renderer::draw_state
    {
        const rendering::command_list &cl;
        /** @brief The key bound, or `no_texture` for the fallback. Meaningless while `layer_bound`. */
        texture_key key = no_texture;
        /** @brief A layer image is bound rather than a registered texture. */
        bool layer_bound = false;
        std::uint32_t mode = k_mode_solid;
        float opacity = 1.0f;

        void set_mode(std::uint32_t new_mode, float new_opacity) noexcept
        {
            if (mode == new_mode && opacity == new_opacity)
                return;
            mode = new_mode;
            opacity = new_opacity;
            const push_block pc{{0.0f, 0.0f}, {0.0f, 0.0f}, mode, opacity};
            rendering::push_constants(cl, k_mode_offset,
                                      std::as_bytes(std::span<const push_block>{&pc, 1}).subspan(k_mode_offset));
        }
    };

    // ---- lifetime -------------------------------------------------------------------------------

    std::expected<renderer, rendering::error> renderer::create(const rendering::device &dev, const renderer_desc &desc)
    {
        using rendering::error_code;
        using rendering::make_error;

        if (!rendering::is_valid(dev) || desc.frames_in_flight == 0 || desc.initial_vertices == 0 ||
            desc.initial_indices == 0)
            return std::unexpected(make_error(error_code::invalid_argument));

        const rendering::device_info info = rendering::get_device_info(dev);
        if (rendering::native_bytecode_format(info.backend) != rendering::shader_bytecode_format::spirv)
            return std::unexpected(make_error(error_code::unsupported_operation));

        // Shaders. The pipeline keeps no reference to its modules, so they go as soon as it exists.
        rendering::shader_desc vs_desc;
        vs_desc.stage = rendering::shader_stage::vertex;
        vs_desc.bytecode = detail::spirv::ui_vertex_bytes();
        vs_desc.debug_name = "ui.vert";
        rendering::shader vs = rendering::create_shader(dev, vs_desc);

        rendering::shader_desc fs_desc;
        fs_desc.stage = rendering::shader_stage::fragment;
        fs_desc.bytecode = detail::spirv::ui_fragment_bytes();
        fs_desc.debug_name = "ui.frag";
        rendering::shader fs = rendering::create_shader(dev, fs_desc);

        if (!vs || !fs)
        {
            rendering::destroy_shader(vs);
            rendering::destroy_shader(fs);
            return std::unexpected(make_error(error_code::shader_invalid));
        }

        // Pipeline: one interleaved binding, premultiplied blending, no culling (the builder does not
        // promise a winding), no depth.
        const std::array<rendering::vertex_binding, 1> bindings = {
            rendering::vertex_binding{0, sizeof(vertex), rendering::vertex_input_rate::per_vertex}};
        const std::array<rendering::vertex_attribute, 3> attributes = {
            rendering::vertex_attribute{0, 0, rendering::format::rg32_float, k_position_offset},
            rendering::vertex_attribute{1, 0, rendering::format::rg32_float, k_uv_offset},
            rendering::vertex_attribute{2, 0, rendering::format::rgba8_unorm, k_color_offset},
        };
        const std::array<rendering::format, 1> color_formats = {desc.color_format};
        const std::array<rendering::blend_state, 1> blends = {blend_premultiplied()};

        rendering::graphics_pipeline_desc pd;
        pd.vertex_shader = vs;
        pd.fragment_shader = fs;
        pd.vertex_input = {bindings, attributes};
        pd.topology = rendering::primitive_topology::triangle_list;
        pd.rasterizer.cull = rendering::cull_mode::none;
        pd.color_formats = color_formats;
        pd.color_blend = blends;
        pd.debug_name = desc.debug_name ? desc.debug_name : "ui";
        rendering::pipeline pipeline = rendering::create_graphics_pipeline(dev, pd);

        rendering::destroy_shader(vs);
        rendering::destroy_shader(fs);

        if (!pipeline)
            return std::unexpected(make_error(error_code::pipeline_creation_failed));

        renderer r;
        r.device_ = dev;
        r.pipeline_ = pipeline;
        r.desc_ = desc;

        // The sampler every textured draw uses, and the white texel a solid draw samples so that the
        // slot is never unbound. Bilinear, clamped: an atlas is padded, and a layer is drawn 1:1.
        rendering::sampler_desc sampler;
        sampler.min_filter = rendering::filter_mode::linear;
        sampler.mag_filter = rendering::filter_mode::linear;
        sampler.mip_filter = rendering::filter_mode::nearest;
        sampler.address_u = rendering::address_mode::clamp_to_edge;
        sampler.address_v = rendering::address_mode::clamp_to_edge;
        sampler.address_w = rendering::address_mode::clamp_to_edge;
        sampler.debug_name = "ui sampler";
        r.sampler_ = rendering::create_sampler(dev, sampler);

        rendering::texture_desc white;
        white.extent = {1, 1, 1};
        white.pixel_format = rendering::format::rgba8_unorm;
        white.usage = rendering::texture_usage::sampled;
        white.debug_name = "ui white";
        const std::array<std::byte, 4> white_texel = {std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF},
                                                      std::byte{0xFF}};
        r.white_ = rendering::create_texture(dev, white, white_texel);

        if (!r.sampler_ || !r.white_)
        {
            r.destroy();
            return std::unexpected(make_error(error_code::out_of_device_memory));
        }

        r.slots_.resize(desc.frames_in_flight);
        for (slot_buffers &s : r.slots_)
        {
            if (!r.reserve(s, desc.initial_vertices, desc.initial_indices))
            {
                r.destroy();
                return std::unexpected(make_error(error_code::out_of_device_memory));
            }
        }

        return r;
    }

    renderer::renderer(renderer &&other) noexcept
        : device_(std::exchange(other.device_, {})), pipeline_(std::exchange(other.pipeline_, {})),
          sampler_(std::exchange(other.sampler_, {})), white_(std::exchange(other.white_, {})),
          slots_(std::move(other.slots_)), textures_(std::move(other.textures_)),
          composite_vertices_(std::move(other.composite_vertices_)),
          composite_indices_(std::move(other.composite_indices_)), desc_(other.desc_)
    {
        other.slots_.clear();
        other.textures_.clear();
    }

    renderer &renderer::operator=(renderer &&other) noexcept
    {
        if (this != &other)
        {
            destroy();
            device_ = std::exchange(other.device_, {});
            pipeline_ = std::exchange(other.pipeline_, {});
            sampler_ = std::exchange(other.sampler_, {});
            white_ = std::exchange(other.white_, {});
            slots_ = std::move(other.slots_);
            textures_ = std::move(other.textures_);
            composite_vertices_ = std::move(other.composite_vertices_);
            composite_indices_ = std::move(other.composite_indices_);
            desc_ = other.desc_;
            other.slots_.clear();
            other.textures_.clear();
        }
        return *this;
    }

    renderer::~renderer()
    {
        destroy();
    }

    void renderer::destroy() noexcept
    {
        for (slot_buffers &s : slots_)
        {
            rendering::destroy_buffer(s.vertices);
            rendering::destroy_buffer(s.indices);
            for (layer_target &l : s.layers)
                rendering::destroy_texture(l.texture);
        }
        slots_.clear();
        textures_.clear();
        rendering::destroy_texture(white_);
        rendering::destroy_sampler(sampler_);
        rendering::destroy_pipeline(pipeline_);
        device_ = {};
    }

    // ---- textures -------------------------------------------------------------------------------

    bool renderer::register_texture(texture_key key, const rendering::texture &t)
    {
        if (key == no_texture || !rendering::is_valid(t))
            return false;

        const rendering::texture_desc td = rendering::get_texture_desc(t);
        if (!rendering::has_flag(td.usage, rendering::texture_usage::sampled))
            return false;

        // What the shader makes of the texture follows its format: one channel is coverage, as a
        // glyph atlas is; anything else is colour.
        const std::uint32_t mode = td.pixel_format == rendering::format::r8_unorm ? k_mode_mask : k_mode_color;

        for (texture_entry &e : textures_)
        {
            if (e.key == key)
            {
                e.texture = t;
                e.mode = mode;
                return true;
            }
        }
        textures_.push_back(texture_entry{key, t, mode});
        return true;
    }

    void renderer::unregister_texture(texture_key key) noexcept
    {
        std::erase_if(textures_, [key](const texture_entry &e) { return e.key == key; });
    }

    rendering::texture renderer::texture_of(texture_key key) const noexcept
    {
        const texture_entry *e = find_texture(key);
        return e ? e->texture : rendering::texture{};
    }

    const renderer::texture_entry *renderer::find_texture(texture_key key) const noexcept
    {
        if (key == no_texture)
            return nullptr;
        for (const texture_entry &e : textures_)
            if (e.key == key)
                return &e;
        return nullptr;
    }

    // ---- buffers --------------------------------------------------------------------------------

    bool renderer::reserve(slot_buffers &s, std::uint32_t vertices, std::uint32_t indices)
    {
        // Double rather than fit exactly, so a screen that grows a little each frame does not
        // reallocate each frame.
        if (s.vertex_capacity < vertices)
        {
            const std::uint32_t grown = std::max(vertices, s.vertex_capacity * 2u);
            if (!replace_buffer(device_, rendering::buffer_usage::vertex, sizeof(vertex), grown, "ui vertices",
                                s.vertices))
                return false;
            s.vertex_capacity = grown;
        }

        if (s.index_capacity < indices)
        {
            const std::uint32_t grown = std::max(indices, s.index_capacity * 2u);
            if (!replace_buffer(device_, rendering::buffer_usage::index, sizeof(index), grown, "ui indices", s.indices))
                return false;
            s.index_capacity = grown;
        }

        return true;
    }

    bool renderer::ensure_layer_target(layer_target &target, rendering::extent2d needed)
    {
        if (target.texture && target.capacity.width >= needed.width && target.capacity.height >= needed.height)
            return true;

        // Grow to cover both what this layer held before and what it needs now, so a layer that
        // alternates between two shapes settles after one frame of each.
        rendering::texture_desc td;
        td.extent = {std::max(needed.width, target.capacity.width), std::max(needed.height, target.capacity.height), 1};
        td.pixel_format = desc_.color_format;
        td.usage = rendering::texture_usage::render_target | rendering::texture_usage::sampled;
        td.debug_name = "ui layer";

        rendering::texture fresh = rendering::create_texture(device_, td);
        if (!fresh)
            return false;

        rendering::destroy_texture(target.texture);
        target.texture = fresh;
        target.capacity = {td.extent.width, td.extent.height};
        return true;
    }

    bool renderer::upload(slot_buffers &s, const render_batch &batch, rendering::extent2d viewport)
    {
        if (!layers_well_formed(batch))
            return false;

        const std::size_t layer_count = batch.layers.size();
        const float vw = static_cast<float>(viewport.width);
        const float vh = static_cast<float>(viewport.height);

        // Place every layer: its bounds cut to the viewport and snapped out to whole pixels, so the
        // composite quad maps its image 1:1 onto the target. The quads go after the batch's own
        // geometry in the same buffers.
        s.placements.assign(layer_count, layer_placement{});
        if (s.layers.size() < layer_count)
            s.layers.resize(layer_count);
        composite_vertices_.clear();
        composite_indices_.clear();

        const auto base_vertex = static_cast<index>(batch.vertices.size());
        for (std::size_t i = 0; i < layer_count; ++i)
        {
            const layer &l = batch.layers[i];
            layer_placement &p = s.placements[i];

            float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
            if (layer_drawable(l))
            {
                x0 = std::floor(std::clamp(l.bounds.min.x(), 0.0f, vw));
                y0 = std::floor(std::clamp(l.bounds.min.y(), 0.0f, vh));
                x1 = std::ceil(std::clamp(l.bounds.max.x(), 0.0f, vw));
                y1 = std::ceil(std::clamp(l.bounds.max.y(), 0.0f, vh));
            }
            p.visible = x1 > x0 && y1 > y0;

            point uv_max{0.0f, 0.0f};
            if (p.visible)
            {
                p.x = static_cast<std::int32_t>(x0);
                p.y = static_cast<std::int32_t>(y0);
                p.width = static_cast<std::uint32_t>(x1 - x0);
                p.height = static_cast<std::uint32_t>(y1 - y0);
                if (!ensure_layer_target(s.layers[i], {p.width, p.height}))
                    return false;
                const rendering::extent2d cap = s.layers[i].capacity;
                uv_max = point{static_cast<float>(p.width) / static_cast<float>(cap.width),
                               static_cast<float>(p.height) / static_cast<float>(cap.height)};
            }

            // The quad is emitted whether or not the layer is visible so that layer `i` is always at
            // quad `i`; an invisible one is simply never drawn.
            const index v = base_vertex + static_cast<index>(i * k_quad_vertices);
            const std::uint32_t white = 0xFFFFFFFFu;
            composite_vertices_.push_back(vertex{point{x0, y0}, point{0.0f, 0.0f}, white});
            composite_vertices_.push_back(vertex{point{x1, y0}, point{uv_max.x(), 0.0f}, white});
            composite_vertices_.push_back(vertex{point{x1, y1}, uv_max, white});
            composite_vertices_.push_back(vertex{point{x0, y1}, point{0.0f, uv_max.y()}, white});
            composite_indices_.insert(composite_indices_.end(), {v, v + 1, v + 2, v, v + 2, v + 3});
        }

        const std::size_t vertex_total = batch.vertices.size() + composite_vertices_.size();
        const std::size_t index_total = batch.indices.size() + composite_indices_.size();
        if (!reserve(s, static_cast<std::uint32_t>(vertex_total), static_cast<std::uint32_t>(index_total)))
            return false;

        if (!rendering::write_buffer(s.vertices, 0, std::as_bytes(std::span<const vertex>{batch.vertices})) ||
            !rendering::write_buffer(s.indices, 0, std::as_bytes(std::span<const index>{batch.indices})))
            return false;

        if (!composite_vertices_.empty())
        {
            if (!rendering::write_buffer(s.vertices, batch.vertices.size() * sizeof(vertex),
                                         std::as_bytes(std::span<const vertex>{composite_vertices_})) ||
                !rendering::write_buffer(s.indices, batch.indices.size() * sizeof(index),
                                         std::as_bytes(std::span<const index>{composite_indices_})))
                return false;
        }

        s.prepared_vertices = batch.vertices.size();
        s.prepared_indices = batch.indices.size();
        s.prepared_commands = batch.commands.size();
        s.prepared_layers = layer_count;
        return true;
    }

    std::uint32_t renderer::vertex_capacity(std::uint32_t slot) const noexcept
    {
        return slot < slots_.size() ? slots_[slot].vertex_capacity : 0u;
    }

    std::uint32_t renderer::index_capacity(std::uint32_t slot) const noexcept
    {
        return slot < slots_.size() ? slots_[slot].index_capacity : 0u;
    }

    std::uint32_t renderer::layer_count(std::uint32_t slot) const noexcept
    {
        if (slot >= slots_.size())
            return 0u;
        return static_cast<std::uint32_t>(std::count_if(slots_[slot].layers.begin(), slots_[slot].layers.end(),
                                                        [](const layer_target &l)
                                                        { return static_cast<bool>(l.texture); }));
    }

    // ---- drawing --------------------------------------------------------------------------------

    void renderer::bind_common(const rendering::command_list &cl, const slot_buffers &s, rendering::extent2d target,
                               std::int32_t origin_x, std::int32_t origin_y) const
    {
        const float w = static_cast<float>(target.width);
        const float h = static_cast<float>(target.height);
        // Pixel p of the batch lands at p - origin in the target.
        const push_block pc{
            {2.0f / w, -2.0f / h},
            {-1.0f - 2.0f * static_cast<float>(origin_x) / w, 1.0f + 2.0f * static_cast<float>(origin_y) / h},
            k_mode_solid,
            1.0f};

        rendering::set_pipeline(cl, pipeline_);
        rendering::set_viewport(cl, {0.0f, 0.0f, w, h});
        rendering::push_constants(cl, 0, std::as_bytes(std::span<const push_block>{&pc, 1}));
        rendering::set_vertex_buffer(cl, 0, s.vertices);
        rendering::set_index_buffer(cl, s.indices, rendering::index_type::uint32);
        rendering::set_sampler(cl, k_texture_slot, sampler_);
        rendering::set_texture(cl, k_texture_slot, white_);
    }

    void renderer::draw_range(const rendering::command_list &cl, const slot_buffers &s, const render_batch &batch,
                              layer_id within, rendering::extent2d target, std::int32_t origin_x, std::int32_t origin_y,
                              std::uint32_t composite_first_index) const
    {
        const bool root = within == no_layer;
        const std::size_t first = root ? 0 : batch.layers[within].first_command;
        const std::size_t end = root ? batch.commands.size() : batch.layers[within].end_command;

        draw_state state{cl};

        std::size_t i = first;
        while (i < end)
        {
            const draw_command &command = batch.commands[i];

            if (command.layer == within)
            {
                ++i;
                if (command.index_count == 0)
                    continue;

                rendering::scissor_rect scissor;
                if (!scissor_of(command.clip, target, origin_x, origin_y, scissor))
                    continue;

                if (state.layer_bound || state.key != command.texture)
                {
                    const texture_entry *entry = find_texture(command.texture);
                    rendering::set_texture(cl, k_texture_slot, entry ? entry->texture : white_);
                    state.set_mode(entry ? entry->mode : k_mode_solid, 1.0f);
                    state.key = command.texture;
                    state.layer_bound = false;
                }

                rendering::set_scissor(cl, scissor);
                rendering::draw_indexed(cl, command.index_count, 1, command.first_index, 0, 0);
                continue;
            }

            // The command belongs to a nested layer; find the one that is a direct child of this
            // range, composite it whole, and skip past everything inside it. The climb ends because a
            // parent always has a smaller id than its child (checked by layers_well_formed).
            layer_id child = command.layer;
            while (child != no_layer && batch.layers[child].parent != within)
                child = batch.layers[child].parent;
            if (child == no_layer)
            {
                ++i; // Not inside this range at all: a malformed batch; ignore the command.
                continue;
            }

            const layer &l = batch.layers[child];
            const layer_placement &p = s.placements[child];
            i = std::max(i + 1, static_cast<std::size_t>(l.end_command));
            if (!p.visible)
                continue;

            rendering::scissor_rect scissor;
            const rect placed = rect::from_xywh(static_cast<float>(p.x), static_cast<float>(p.y),
                                                static_cast<float>(p.width), static_cast<float>(p.height));
            if (!scissor_of(placed, target, origin_x, origin_y, scissor))
                continue;

            rendering::set_texture(cl, k_texture_slot, s.layers[child].texture);
            state.set_mode(k_mode_layer, l.opacity);
            state.layer_bound = true;

            rendering::set_scissor(cl, scissor);
            rendering::draw_indexed(cl, k_quad_indices, 1, composite_first_index + child * k_quad_indices, 0, 0);
        }
    }

    bool renderer::prepare(const rendering::command_list &cl, const render_batch &batch, rendering::extent2d viewport,
                           std::uint32_t slot)
    {
        if (!valid() || slot >= slots_.size() || viewport.width == 0 || viewport.height == 0)
            return false;

        slot_buffers &s = slots_[slot];
        s.prepared = false;

        if (batch.empty() || batch.vertices.empty() || batch.indices.empty())
            return true;

        if (!upload(s, batch, viewport))
            return false;

        // Children before parents: a layer's pass samples the images of the layers nested in it, and
        // ids increase from parent to child.
        const auto composite_first_index = static_cast<std::uint32_t>(batch.indices.size());
        for (std::size_t n = batch.layers.size(); n-- > 0;)
        {
            const layer_placement &p = s.placements[n];
            if (!p.visible)
                continue;

            const rendering::color_attachment attachment{
                .target = s.layers[n].texture,
                .load = rendering::load_op::clear,
                .store = rendering::store_op::store,
                .clear = {0.0f, 0.0f, 0.0f, 0.0f},
            };
            rendering::begin_render_pass(cl,
                                         {.color_attachments = std::span{&attachment, 1}, .debug_name = "ui layer"});

            const rendering::extent2d target{p.width, p.height};
            bind_common(cl, s, target, p.x, p.y);
            draw_range(cl, s, batch, static_cast<layer_id>(n), target, p.x, p.y, composite_first_index);

            rendering::end_render_pass(cl);
        }

        s.prepared = true;
        return true;
    }

    bool renderer::render(const rendering::command_list &cl, const render_batch &batch, rendering::extent2d viewport,
                          std::uint32_t slot)
    {
        if (!valid() || slot >= slots_.size() || viewport.width == 0 || viewport.height == 0)
            return false;

        slot_buffers &s = slots_[slot];
        const bool prepared = std::exchange(s.prepared, false);

        if (batch.empty() || batch.vertices.empty() || batch.indices.empty())
            return true;

        if (prepared)
        {
            // `prepare` must have seen this batch: the images and composite quads belong to it.
            if (s.prepared_vertices != batch.vertices.size() || s.prepared_indices != batch.indices.size() ||
                s.prepared_commands != batch.commands.size() || s.prepared_layers != batch.layers.size())
                return false;
        }
        else
        {
            if (!batch.layers.empty())
                return false; // Layers need their offscreen passes, which only `prepare` records.
            if (!upload(s, batch, viewport))
                return false;
        }

        bind_common(cl, s, viewport, 0, 0);
        draw_range(cl, s, batch, no_layer, viewport, 0, 0, static_cast<std::uint32_t>(batch.indices.size()));
        return true;
    }

} // namespace catalyst::ui
