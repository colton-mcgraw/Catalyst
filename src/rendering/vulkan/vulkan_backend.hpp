/**
 * SPDX-License-Identifier: CDDL-1.0
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Internal header of the Vulkan rendering backend: the per-resource state records, the id registry and the
 * helpers shared between the translation units that implement src/rendering/detail_backend.hpp on top of Vulkan 1.3.
 * @details Design in one paragraph so the individual files make sense:
 *   - One `VkInstance` + `VkDevice` per `device`; everything runs on a single graphics/compute queue.
 *   - Render passes use dynamic rendering (core 1.3): no VkRenderPass / VkFramebuffer objects.
 *   - Image layouts follow a fixed invariant instead of being tracked: user textures always sit in
 *     `VK_IMAGE_LAYOUT_GENERAL`, presentable swapchain images in `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` outside a render
 *     pass and `COLOR_ATTACHMENT_OPTIMAL` inside one. Hazards are covered by a conservative full barrier before each
 *     render pass, dispatch and copy.
 *   - Resource binding uses one pipeline layout for every pipeline with four descriptor sets: uniform buffers (set 0),
 *     storage buffers (set 1), sampled textures (set 2) and samplers (set 3); the public `slot` is the binding index.
 *     Sets are partially bound, so unbound slots are simply not written. 128 bytes of push constants are visible to all
 *     stages.
 *   - Submissions are tracked with a monotonically increasing serial and one fence each. Destroying a resource while
 *     work is in flight defers the Vulkan release until every submission issued so far has completed.
 *   - Synchronous uploads (initial data, `write_buffer` on GPU-only memory) use a dedicated "immediate" command buffer
 *     that is submitted and waited on inline.
 * Not part of the public API.
 */

#pragma once

#if defined(_WIN32)
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <vulkan/vulkan.h>

#include "../detail_backend.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace catalyst::rendering::detail::vulkan
{

    // -------------------------------------------------------------------------
    // Binding model
    // -------------------------------------------------------------------------

    inline constexpr std::uint32_t set_uniform_buffers = 0;
    inline constexpr std::uint32_t set_storage_buffers = 1;
    inline constexpr std::uint32_t set_textures = 2;
    inline constexpr std::uint32_t set_samplers = 3;
    inline constexpr std::uint32_t descriptor_set_count = 4;

    /** Descriptor sets allocated per pool before a command list grows a new one. */
    inline constexpr std::uint32_t descriptor_sets_per_pool = 256;

    // -------------------------------------------------------------------------
    // Resource records
    // -------------------------------------------------------------------------

    struct submission
    {
        VkFence fence = VK_NULL_HANDLE;
        std::uint64_t serial = 0;
    };

    struct deferred_release
    {
        std::uint64_t serial = 0;
        std::function<void()> release;
    };

    struct device_features
    {
        bool sampler_anisotropy = false;
        bool fill_mode_non_solid = false;
        bool depth_clamp = false;
    };

    /**
     * Persistently mapped transfer-source buffer. One is owned by each device and grown on demand rather than
     * allocated per transfer: `vkAllocateMemory` / `vkFreeMemory` dominate the cost of a small staged upload.
     */
    struct staging_buffer
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void *mapped = nullptr;
        VkDeviceSize capacity = 0;
        bool coherent = true;
    };

    struct device_state
    {
        device_desc desc;
        std::string application_name;

        VkInstance instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkQueue queue = VK_NULL_HANDLE;
        std::uint32_t queue_family = 0;

        VkPhysicalDeviceProperties properties{};
        VkPhysicalDeviceMemoryProperties memory_properties{};
        std::string adapter_name;
        std::uint64_t dedicated_video_memory = 0;
        device_features features;
        /** What `format::d24_unorm_s8_uint` resolves to on this adapter (itself, or d32_float_s8_uint). */
        format depth_stencil_format = format::d24_unorm_s8_uint;

        bool debug_utils = false;
        PFN_vkSetDebugUtilsObjectNameEXT set_object_name = nullptr;

        std::array<VkDescriptorSetLayout, descriptor_set_count> set_layouts{};
        VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

        /** Serial of the most recent submission; 0 when nothing has been submitted yet. */
        std::uint64_t last_serial = 0;
        /** Highest serial known to have completed on the GPU. */
        std::uint64_t completed_serial = 0;
        std::vector<submission> in_flight;
        std::vector<VkFence> free_fences;
        std::vector<deferred_release> garbage;

        VkCommandPool immediate_pool = VK_NULL_HANDLE;
        VkCommandBuffer immediate_cmd = VK_NULL_HANDLE;
        VkFence immediate_fence = VK_NULL_HANDLE;
        /** Reused upload buffer for staged transfers; only ever touched by the immediate command buffer. */
        staging_buffer staging;

        /**
         * True when device-local memory is host-visible, as on integrated adapters where the device-local heap is
         * system RAM. `gpu_only` buffers are then mapped and written directly instead of through a staging copy.
         */
        bool unified_memory = false;

        /** Swapchains holding an acquired image whose acquire semaphore no submission has waited on yet. */
        std::vector<resource_id> pending_acquires;
    };

    struct buffer_state
    {
        resource_id owner = 0;
        buffer_desc desc;
        std::string debug_name;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        /**
         * Persistent mapping when the allocation landed in host-visible memory, else null. `gpu_only` buffers are
         * mapped too on unified-memory adapters, which is what lets `write_buffer` avoid staging there.
         */
        void *mapped = nullptr;
        bool coherent = true;
    };

    struct shader_state
    {
        resource_id owner = 0;
        shader_stage stage = shader_stage::vertex;
        shader_bytecode_format bytecode_format = shader_bytecode_format::spirv;
        std::string entry_point;
        std::string debug_name;
        std::vector<std::byte> bytecode;
        VkShaderModule module = VK_NULL_HANDLE;
    };

    struct texture_state
    {
        resource_id owner = 0;
        texture_desc desc;
        std::string debug_name;
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        /** View covering every aspect; used for attachments. */
        VkImageView view = VK_NULL_HANDLE;
        /** Depth-only view for depth/stencil formats, otherwise equal to `view`; used for sampling. */
        VkImageView sampled_view = VK_NULL_HANDLE;
        VkFormat vk_format = VK_FORMAT_UNDEFINED;
        VkImageAspectFlags aspect = 0;
        /** Non-zero when the texture is a back buffer owned by that swapchain. */
        resource_id swapchain = 0;
        /** True when `image` belongs to a VkSwapchainKHR (and lives in PRESENT_SRC outside render passes). */
        bool presentable = false;
        std::uint32_t image_index = 0;
    };

    struct sampler_state
    {
        resource_id owner = 0;
        sampler_desc desc;
        VkSampler sampler = VK_NULL_HANDLE;
    };

    struct pipeline_state
    {
        resource_id owner = 0;
        pipeline_type type = pipeline_type::graphics;
        VkPipeline pipeline = VK_NULL_HANDLE;
    };

    struct swapchain_state
    {
        resource_id owner = 0;
        swapchain_desc desc;
        std::string debug_name;

        /** False for window-less swapchains, which emulate presentation with a ring of ordinary render targets. */
        bool windowed = false;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VkFormat vk_format = VK_FORMAT_UNDEFINED;
        VkColorSpaceKHR color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

        /** Texture ids of the back buffers, in image-index order. */
        std::vector<resource_id> images;

        /** Ring of acquire semaphores and the serial of the submission that consumed each. */
        std::vector<VkSemaphore> acquire_semaphores;
        std::vector<std::uint64_t> acquire_serials;
        std::uint32_t acquire_slot = 0;
        /** Per image: signalled by the submission that consumed the acquire, waited on by the present. */
        std::vector<VkSemaphore> render_finished;
        /** Per image: used when work was submitted after `render_finished` was signalled (see `present`). */
        std::vector<VkSemaphore> present_ready;

        std::uint32_t current_image = 0;
        bool acquired = false;
        /** The current image's acquire semaphore has not been waited on by any submission yet. */
        bool acquire_pending = false;
        /** Serial of the submission that signalled `render_finished[current_image]`. */
        std::uint64_t render_finished_serial = 0;
        /** Set when acquire / present reported the surface changed; `acquire_next_image` fails until a resize. */
        bool out_of_date = false;

        /** Window-less swapchains: next image to hand out. */
        std::uint32_t next_image = 0;
    };

    struct buffer_binding
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset = 0;
        VkDeviceSize range = 0;
    };

    struct command_list_state
    {
        resource_id owner = 0;
        command_list_desc desc;
        std::string debug_name;

        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        std::vector<VkDescriptorPool> descriptor_pools;
        std::size_t active_pool = 0;

        bool recording = false;
        bool ready = false;
        bool in_render_pass = false;
        resource_id bound_pipeline = 0;
        pipeline_type bound_pipeline_type = pipeline_type::graphics;
        /** Serial of the last submission that included this list; 0 when never submitted. */
        std::uint64_t last_submit_serial = 0;

        /** Presentable images bound as attachments of the open render pass (transitioned back at end). */
        std::vector<VkImage> pass_present_images;

        std::array<buffer_binding, max_uniform_buffer_slots> uniform_buffers{};
        std::array<buffer_binding, max_storage_buffer_slots> storage_buffers{};
        std::array<VkImageView, max_texture_slots> textures{};
        std::array<VkSampler, max_sampler_slots> samplers{};
        /** Per set: bindings changed since the set was last allocated. */
        std::array<bool, descriptor_set_count> dirty{};
        /** Per set: the most recently allocated descriptor set (null until first use). */
        std::array<VkDescriptorSet, descriptor_set_count> sets{};
        /** Per set: whether `sets[i]` is currently bound to the graphics / compute bind point. */
        std::array<bool, descriptor_set_count> bound_graphics{};
        std::array<bool, descriptor_set_count> bound_compute{};
    };

    // -------------------------------------------------------------------------
    // Registry
    // -------------------------------------------------------------------------

    struct registry
    {
        resource_id next_id = 1;
        std::unordered_map<resource_id, device_state> devices;
        std::unordered_map<resource_id, buffer_state> buffers;
        std::unordered_map<resource_id, shader_state> shaders;
        std::unordered_map<resource_id, texture_state> textures;
        std::unordered_map<resource_id, sampler_state> samplers;
        std::unordered_map<resource_id, pipeline_state> pipelines;
        std::unordered_map<resource_id, swapchain_state> swapchains;
        std::unordered_map<resource_id, command_list_state> command_lists;
    };

    registry &reg() noexcept;
    resource_id allocate_id() noexcept;

    template <typename Map>
    typename Map::mapped_type *find(Map &map, resource_id id) noexcept
    {
        if (id == 0)
            return nullptr;
        auto it = map.find(id);
        return it == map.end() ? nullptr : &it->second;
    }

    inline device_state *find_device(resource_id id) noexcept
    {
        return find(reg().devices, id);
    }

    inline std::string copy_name(const char *name)
    {
        return name ? std::string{name} : std::string{};
    }

    inline const char *name_or_null(const std::string &name) noexcept
    {
        return name.empty() ? nullptr : name.c_str();
    }

    inline bool range_in_bounds(std::size_t size, std::size_t offset, std::size_t length) noexcept
    {
        return offset <= size && length <= size - offset;
    }

    /** Vulkan handle as the 64-bit integer VK_EXT_debug_utils expects (handles are pointers on 64-bit targets). */
    template <typename Handle>
    std::uint64_t handle_bits(Handle h) noexcept
    {
        if constexpr (std::is_pointer_v<Handle>)
            return reinterpret_cast<std::uint64_t>(h);
        else
            return static_cast<std::uint64_t>(h);
    }

    // -------------------------------------------------------------------------
    // Diagnostics (vulkan_device.cpp)
    // -------------------------------------------------------------------------

    /** Prints a formatted line to stderr prefixed with the backend name. */
    void report(const char *fmt, ...) noexcept;
    const char *result_string(VkResult result) noexcept;
    void set_debug_name(device_state &dev, VkObjectType type, std::uint64_t handle, const char *name) noexcept;

    inline void set_debug_name(device_state &dev, VkObjectType type, std::uint64_t handle,
                               const std::string &name) noexcept
    {
        if (!name.empty())
            set_debug_name(dev, type, handle, name.c_str());
    }

    // -------------------------------------------------------------------------
    // Submission tracking (vulkan_device.cpp)
    // -------------------------------------------------------------------------

    /**
     * Submits `commands` on the device queue with a fresh fence and returns the submission's serial (0 on failure).
     * `waits` / `wait_stages` / `signals` are forwarded to VkSubmitInfo.
     */
    std::uint64_t submit_batch(device_state &dev, std::span<const VkCommandBuffer> commands,
                               std::span<const VkSemaphore> waits, std::span<const VkPipelineStageFlags> wait_stages,
                               std::span<const VkSemaphore> signals) noexcept;
    /** Retires every in-flight submission whose fence has signalled and runs the deferred releases they unblock. */
    void poll_submissions(device_state &dev) noexcept;
    /** Blocks until the submission with `serial` (and everything before it) has completed. */
    void wait_for_serial(device_state &dev, std::uint64_t serial) noexcept;
    /** Blocks until the device is idle. */
    void wait_all(device_state &dev) noexcept;
    /** Runs `release` now if nothing is in flight, otherwise once every current submission has completed. */
    void defer_release(device_state &dev, std::function<void()> release);

    /**
     * Begins the device's immediate command buffer. Pair with `end_immediate`, which submits it and waits for
     * completion. Returns VK_NULL_HANDLE on failure.
     */
    VkCommandBuffer begin_immediate(device_state &dev) noexcept;
    bool end_immediate(device_state &dev) noexcept;

    /** Records an all-commands / all-memory pipeline barrier. */
    void full_barrier(VkCommandBuffer cmd) noexcept;

    /** Substitutes formats the adapter cannot use (currently only the depth/stencil pair). */
    format resolve_format(const device_state &dev, format f) noexcept;

    // -------------------------------------------------------------------------
    // Memory (vulkan_memory.cpp)
    // -------------------------------------------------------------------------

    bool find_memory_type(const device_state &dev, std::uint32_t type_bits, VkMemoryPropertyFlags required,
                          VkMemoryPropertyFlags preferred, std::uint32_t &out_index) noexcept;

    /** Allocates and binds memory for `buffer` according to `access`; maps it persistently when host-visible. */
    bool allocate_buffer_memory(device_state &dev, VkBuffer buffer, memory_access access, VkDeviceMemory &out_memory,
                                void *&out_mapped, bool &out_coherent) noexcept;

    /** Allocates and binds device-local memory for `image`. */
    bool allocate_image_memory(device_state &dev, VkImage image, VkDeviceMemory &out_memory) noexcept;

    /** Flush / invalidate the whole mapped range of a non-coherent allocation (no-ops are cheap enough to skip checks). */
    void flush_host_writes(device_state &dev, VkDeviceMemory memory) noexcept;
    void invalidate_host_reads(device_state &dev, VkDeviceMemory memory) noexcept;

    /**
     * Copies `data` into the device's staging buffer (growing it first if needed) and reports the buffer to use as
     * the transfer source. Valid until the next staged transfer on this device; the immediate command buffer is
     * submitted and waited on before control returns to the caller, so successive transfers cannot overlap.
     */
    bool stage_upload(device_state &dev, std::span<const std::byte> data, VkBuffer &out_source) noexcept;
    void release_staging(device_state &dev) noexcept;

    // -------------------------------------------------------------------------
    // Per-resource release hooks used by destroy_device (each in its own file)
    // -------------------------------------------------------------------------

    void release_buffer_objects(device_state &dev, buffer_state &b) noexcept;
    void release_shader_objects(device_state &dev, shader_state &s) noexcept;
    void release_texture_objects(device_state &dev, texture_state &t) noexcept;
    void release_sampler_objects(device_state &dev, sampler_state &s) noexcept;
    void release_pipeline_objects(device_state &dev, pipeline_state &p) noexcept;
    void release_swapchain_objects(device_state &dev, swapchain_state &sc) noexcept;
    void release_command_list_objects(device_state &dev, command_list_state &cl) noexcept;

    // -------------------------------------------------------------------------
    // Cross-file texture helpers (vulkan_texture.cpp)
    // -------------------------------------------------------------------------

    /**
     * Creates a texture owned by `device_id`, optionally uploading `initial_data` to mip 0 / layer 0, and registers it
     * with `swapchain_id` (0 for user textures). The image is left in VK_IMAGE_LAYOUT_GENERAL.
     */
    resource_id create_texture_internal(resource_id device_id, const texture_desc &desc,
                                        std::span<const std::byte> initial_data, resource_id swapchain_id);

    /** Registers a VkSwapchainKHR image as a texture (creates its view; the image itself is not owned). */
    resource_id register_presentable_image(resource_id device_id, resource_id swapchain_id, std::uint32_t index,
                                           VkImage image, VkFormat vk_format, const texture_desc &desc,
                                           const std::string &debug_name);

    /** Destroys a swapchain-owned texture record immediately (the device must be idle). */
    void destroy_swapchain_texture(device_state &dev, resource_id texture_id) noexcept;

    // -------------------------------------------------------------------------
    // Swapchain <-> submit handoff (vulkan_swapchain.cpp)
    // -------------------------------------------------------------------------

    /**
     * Appends, for every swapchain in `dev.pending_acquires`, the acquire semaphore a submission must wait on and the
     * render-finished semaphore it must signal. Call before `submit_batch`, then `complete_acquire_waits` with the
     * resulting serial.
     */
    void collect_acquire_waits(device_state &dev, std::vector<VkSemaphore> &waits,
                               std::vector<VkPipelineStageFlags> &wait_stages, std::vector<VkSemaphore> &signals);
    void complete_acquire_waits(device_state &dev, std::uint64_t serial) noexcept;

} // namespace catalyst::rendering::detail::vulkan
