/**
 * SPDX-License-Identifier: CDDL-1.0
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Device layer of the Vulkan backend: instance and adapter selection, logical device and queue creation, the
 * shared pipeline layout, fence-based submission tracking with deferred resource release, the synchronous "immediate"
 * command buffer used for uploads, and the `destroy_device` cascade over every owned resource.
 */

#include "vulkan_backend.hpp"
#include "vulkan_convert.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <utility>

namespace catalyst::rendering::detail::vulkan
{

    // -------------------------------------------------------------------------
    // Diagnostics
    // -------------------------------------------------------------------------

    void report(const char *fmt, ...) noexcept
    {
        std::fputs("[catalyst.rendering.vulkan] ", stderr);
        va_list args;
        va_start(args, fmt);
        std::vfprintf(stderr, fmt, args);
        va_end(args);
        std::fputc('\n', stderr);
    }

    const char *result_string(VkResult result) noexcept
    {
        switch (result)
        {
        case VK_SUCCESS:                        return "VK_SUCCESS";
        case VK_NOT_READY:                      return "VK_NOT_READY";
        case VK_TIMEOUT:                        return "VK_TIMEOUT";
        case VK_INCOMPLETE:                     return "VK_INCOMPLETE";
        case VK_SUBOPTIMAL_KHR:                 return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_HOST_MEMORY:       return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:     return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED:    return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST:              return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED:        return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT:        return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT:    return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT:      return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER:      return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS:         return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:     return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_SURFACE_LOST_KHR:         return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR:          return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT:    return "VK_ERROR_VALIDATION_FAILED_EXT";
        case VK_ERROR_INVALID_SHADER_NV:        return "VK_ERROR_INVALID_SHADER_NV";
        case VK_ERROR_UNKNOWN:                  return "VK_ERROR_UNKNOWN";
        default:                                return "VkResult(other)";
        }
    }

    void set_debug_name(device_state &dev, VkObjectType type, std::uint64_t handle, const char *name) noexcept
    {
        if (!dev.set_object_name || !name || !*name || handle == 0)
            return;
        VkDebugUtilsObjectNameInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        info.objectType = type;
        info.objectHandle = handle;
        info.pObjectName = name;
        dev.set_object_name(dev.device, &info);
    }

    format resolve_format(const device_state &dev, format f) noexcept
    {
        if (f == format::d24_unorm_s8_uint)
            return dev.depth_stencil_format;
        return f;
    }

    namespace
    {
        // ---------------------------------------------------------------------
        // Instance
        // ---------------------------------------------------------------------

        VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                                VkDebugUtilsMessageTypeFlagsEXT /*types*/,
                                                                const VkDebugUtilsMessengerCallbackDataEXT *data,
                                                                void * /*user*/)
        {
            const char *level = "info";
            if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
                level = "error";
            else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
                level = "warning";
            report("validation %s: %s", level, data && data->pMessage ? data->pMessage : "");
            return VK_FALSE;
        }

        bool has_instance_layer(const char *name)
        {
            std::uint32_t count = 0;
            if (vkEnumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS)
                return false;
            std::vector<VkLayerProperties> layers(count);
            if (vkEnumerateInstanceLayerProperties(&count, layers.data()) != VK_SUCCESS)
                return false;
            return std::any_of(layers.begin(), layers.end(),
                               [name](const VkLayerProperties &l) { return std::strcmp(l.layerName, name) == 0; });
        }

        bool has_instance_extension(const char *name)
        {
            std::uint32_t count = 0;
            if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS)
                return false;
            std::vector<VkExtensionProperties> extensions(count);
            if (vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data()) != VK_SUCCESS)
                return false;
            return std::any_of(extensions.begin(), extensions.end(), [name](const VkExtensionProperties &e) {
                return std::strcmp(e.extensionName, name) == 0;
            });
        }

        bool has_device_extension(VkPhysicalDevice pd, const char *name)
        {
            std::uint32_t count = 0;
            if (vkEnumerateDeviceExtensionProperties(pd, nullptr, &count, nullptr) != VK_SUCCESS)
                return false;
            std::vector<VkExtensionProperties> extensions(count);
            if (vkEnumerateDeviceExtensionProperties(pd, nullptr, &count, extensions.data()) != VK_SUCCESS)
                return false;
            return std::any_of(extensions.begin(), extensions.end(), [name](const VkExtensionProperties &e) {
                return std::strcmp(e.extensionName, name) == 0;
            });
        }

        bool create_instance(device_state &dev)
        {
            std::uint32_t loader_version = VK_API_VERSION_1_0;
            vkEnumerateInstanceVersion(&loader_version);
            if (loader_version < VK_API_VERSION_1_3)
            {
                report("create_device: the Vulkan loader only supports %u.%u, 1.3 is required",
                       VK_API_VERSION_MAJOR(loader_version), VK_API_VERSION_MINOR(loader_version));
                return false;
            }

            VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
            app.pApplicationName = dev.application_name.c_str();
            app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            app.pEngineName = "Catalyst";
            app.engineVersion = VK_MAKE_VERSION(0, 1, 0);
            app.apiVersion = VK_API_VERSION_1_3;

            std::vector<const char *> layers;
            std::vector<const char *> extensions = {VK_KHR_SURFACE_EXTENSION_NAME};
#if defined(VK_USE_PLATFORM_WIN32_KHR)
            extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif

            if (dev.desc.enable_validation)
            {
                if (has_instance_layer("VK_LAYER_KHRONOS_validation"))
                    layers.push_back("VK_LAYER_KHRONOS_validation");
                else
                    report("create_device: validation requested but VK_LAYER_KHRONOS_validation is not installed");

                if (has_instance_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
                {
                    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                    dev.debug_utils = true;
                }
            }

            for (const char *ext : extensions)
            {
                if (!has_instance_extension(ext))
                {
                    report("create_device: required instance extension %s is unavailable", ext);
                    return false;
                }
            }

            VkDebugUtilsMessengerCreateInfoEXT messenger_info{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                             VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                         VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                         VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            messenger_info.pfnUserCallback = debug_messenger_callback;

            VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
            info.pApplicationInfo = &app;
            info.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
            info.ppEnabledLayerNames = layers.data();
            info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
            info.ppEnabledExtensionNames = extensions.data();
            if (dev.debug_utils)
                info.pNext = &messenger_info; // Also covers vkCreateInstance / vkDestroyInstance.

            const VkResult result = vkCreateInstance(&info, nullptr, &dev.instance);
            if (result != VK_SUCCESS)
            {
                report("create_device: vkCreateInstance failed (%s)", result_string(result));
                return false;
            }

            if (dev.debug_utils)
            {
                auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                    vkGetInstanceProcAddr(dev.instance, "vkCreateDebugUtilsMessengerEXT"));
                if (create)
                    create(dev.instance, &messenger_info, nullptr, &dev.messenger);
            }
            return true;
        }

        // ---------------------------------------------------------------------
        // Adapter selection
        // ---------------------------------------------------------------------

        struct adapter_candidate
        {
            VkPhysicalDevice physical_device = VK_NULL_HANDLE;
            std::uint32_t queue_family = 0;
            int score = -1;
            VkPhysicalDeviceProperties properties{};
        };

        bool find_queue_family(VkPhysicalDevice pd, std::uint32_t &out_family)
        {
            std::uint32_t count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(pd, &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            vkGetPhysicalDeviceQueueFamilyProperties(pd, &count, families.data());

            constexpr VkQueueFlags required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
            for (std::uint32_t i = 0; i < count; ++i)
            {
                if ((families[i].queueFlags & required) != required)
                    continue;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
                if (!vkGetPhysicalDeviceWin32PresentationSupportKHR(pd, i))
                    continue;
#endif
                out_family = i;
                return true;
            }
            return false;
        }

        bool has_required_features(VkPhysicalDevice pd)
        {
            VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
            VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
            f12.pNext = &f13;
            VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
            f2.pNext = &f12;
            vkGetPhysicalDeviceFeatures2(pd, &f2);
            return f13.dynamicRendering && f12.descriptorBindingPartiallyBound;
        }

        bool pick_adapter(const device_state &dev, adapter_candidate &out)
        {
            std::uint32_t count = 0;
            if (vkEnumeratePhysicalDevices(dev.instance, &count, nullptr) != VK_SUCCESS || count == 0)
            {
                report("create_device: no Vulkan physical devices found");
                return false;
            }
            std::vector<VkPhysicalDevice> devices(count);
            vkEnumeratePhysicalDevices(dev.instance, &count, devices.data());

            adapter_candidate best;
            for (VkPhysicalDevice pd : devices)
            {
                adapter_candidate c;
                c.physical_device = pd;
                vkGetPhysicalDeviceProperties(pd, &c.properties);

                if (c.properties.apiVersion < VK_API_VERSION_1_3)
                    continue;
                if (!find_queue_family(pd, c.queue_family))
                    continue;
                if (!has_device_extension(pd, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
                    continue;
                if (!has_required_features(pd))
                    continue;

                switch (c.properties.deviceType)
                {
                case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   c.score = dev.desc.prefer_discrete_adapter ? 3 : 1; break;
                case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: c.score = 2; break;
                case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    c.score = 1; break;
                default:                                     c.score = 0; break;
                }

                if (c.score > best.score)
                    best = c;
            }

            if (best.score < 0)
            {
                report("create_device: no adapter supports Vulkan 1.3 with dynamic rendering, partially bound "
                       "descriptors and a graphics+compute queue that can present");
                return false;
            }
            out = best;
            return true;
        }

        // ---------------------------------------------------------------------
        // Logical device
        // ---------------------------------------------------------------------

        bool create_logical_device(device_state &dev, const adapter_candidate &adapter)
        {
            dev.physical_device = adapter.physical_device;
            dev.queue_family = adapter.queue_family;
            dev.properties = adapter.properties;
            dev.adapter_name = adapter.properties.deviceName;
            vkGetPhysicalDeviceMemoryProperties(dev.physical_device, &dev.memory_properties);

            dev.dedicated_video_memory = 0;
            for (std::uint32_t i = 0; i < dev.memory_properties.memoryHeapCount; ++i)
            {
                if (dev.memory_properties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                    dev.dedicated_video_memory += dev.memory_properties.memoryHeaps[i].size;
            }

            VkFormatProperties d24s8{};
            vkGetPhysicalDeviceFormatProperties(dev.physical_device, VK_FORMAT_D24_UNORM_S8_UINT, &d24s8);
            dev.depth_stencil_format = (d24s8.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
                                           ? format::d24_unorm_s8_uint
                                           : format::d32_float_s8_uint;

            VkPhysicalDeviceFeatures supported{};
            vkGetPhysicalDeviceFeatures(dev.physical_device, &supported);
            dev.features.sampler_anisotropy = supported.samplerAnisotropy == VK_TRUE;
            dev.features.fill_mode_non_solid = supported.fillModeNonSolid == VK_TRUE;
            dev.features.depth_clamp = supported.depthClamp == VK_TRUE;

            VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
            f13.dynamicRendering = VK_TRUE;
            VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
            f12.pNext = &f13;
            f12.descriptorBindingPartiallyBound = VK_TRUE;
            VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
            f2.pNext = &f12;
            f2.features.samplerAnisotropy = supported.samplerAnisotropy;
            f2.features.fillModeNonSolid = supported.fillModeNonSolid;
            f2.features.depthClamp = supported.depthClamp;

            const float priority = 1.0f;
            VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queue_info.queueFamilyIndex = dev.queue_family;
            queue_info.queueCount = 1;
            queue_info.pQueuePriorities = &priority;

            const char *extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

            VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            info.pNext = &f2;
            info.queueCreateInfoCount = 1;
            info.pQueueCreateInfos = &queue_info;
            info.enabledExtensionCount = 1;
            info.ppEnabledExtensionNames = extensions;

            const VkResult result = vkCreateDevice(dev.physical_device, &info, nullptr, &dev.device);
            if (result != VK_SUCCESS)
            {
                report("create_device: vkCreateDevice failed (%s)", result_string(result));
                return false;
            }

            vkGetDeviceQueue(dev.device, dev.queue_family, 0, &dev.queue);

            if (dev.debug_utils)
            {
                dev.set_object_name = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
                    vkGetDeviceProcAddr(dev.device, "vkSetDebugUtilsObjectNameEXT"));
            }
            return true;
        }

        // ---------------------------------------------------------------------
        // Shared pipeline layout
        // ---------------------------------------------------------------------

        VkDescriptorType descriptor_type_for_set(std::uint32_t set) noexcept
        {
            switch (set)
            {
            case set_uniform_buffers: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            case set_storage_buffers: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            case set_textures:        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            default:                  return VK_DESCRIPTOR_TYPE_SAMPLER;
            }
        }

        std::uint32_t slots_for_set(std::uint32_t set) noexcept
        {
            switch (set)
            {
            case set_uniform_buffers: return max_uniform_buffer_slots;
            case set_storage_buffers: return max_storage_buffer_slots;
            case set_textures:        return max_texture_slots;
            default:                  return max_sampler_slots;
            }
        }

        bool create_pipeline_layout(device_state &dev)
        {
            for (std::uint32_t set = 0; set < descriptor_set_count; ++set)
            {
                const std::uint32_t slots = slots_for_set(set);
                std::vector<VkDescriptorSetLayoutBinding> bindings(slots);
                std::vector<VkDescriptorBindingFlags> flags(slots, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
                for (std::uint32_t i = 0; i < slots; ++i)
                {
                    bindings[i].binding = i;
                    bindings[i].descriptorType = descriptor_type_for_set(set);
                    bindings[i].descriptorCount = 1;
                    bindings[i].stageFlags = VK_SHADER_STAGE_ALL;
                }

                VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info{
                    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
                flags_info.bindingCount = slots;
                flags_info.pBindingFlags = flags.data();

                VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
                info.pNext = &flags_info;
                info.bindingCount = slots;
                info.pBindings = bindings.data();

                const VkResult result = vkCreateDescriptorSetLayout(dev.device, &info, nullptr, &dev.set_layouts[set]);
                if (result != VK_SUCCESS)
                {
                    report("create_device: vkCreateDescriptorSetLayout failed (%s)", result_string(result));
                    return false;
                }
            }

            VkPushConstantRange push_range{};
            push_range.stageFlags = VK_SHADER_STAGE_ALL;
            push_range.offset = 0;
            push_range.size = max_push_constant_bytes;

            VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            info.setLayoutCount = descriptor_set_count;
            info.pSetLayouts = dev.set_layouts.data();
            info.pushConstantRangeCount = 1;
            info.pPushConstantRanges = &push_range;

            const VkResult result = vkCreatePipelineLayout(dev.device, &info, nullptr, &dev.pipeline_layout);
            if (result != VK_SUCCESS)
            {
                report("create_device: vkCreatePipelineLayout failed (%s)", result_string(result));
                return false;
            }
            return true;
        }

        // ---------------------------------------------------------------------
        // Immediate command buffer
        // ---------------------------------------------------------------------

        bool create_immediate_objects(device_state &dev)
        {
            VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            pool_info.queueFamilyIndex = dev.queue_family;
            if (vkCreateCommandPool(dev.device, &pool_info, nullptr, &dev.immediate_pool) != VK_SUCCESS)
                return false;

            VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            alloc.commandPool = dev.immediate_pool;
            alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            alloc.commandBufferCount = 1;
            if (vkAllocateCommandBuffers(dev.device, &alloc, &dev.immediate_cmd) != VK_SUCCESS)
                return false;

            VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            return vkCreateFence(dev.device, &fence_info, nullptr, &dev.immediate_fence) == VK_SUCCESS;
        }

        // ---------------------------------------------------------------------
        // Teardown
        // ---------------------------------------------------------------------

        template <typename Map, typename Release>
        void release_owned(Map &map, resource_id device, Release &&release)
        {
            for (auto it = map.begin(); it != map.end();)
            {
                if (it->second.owner == device)
                {
                    release(it->second);
                    it = map.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        void destroy_device_objects(device_state &dev) noexcept
        {
            if (dev.device)
            {
                vkDeviceWaitIdle(dev.device);

                for (deferred_release &g : dev.garbage)
                    g.release();
                dev.garbage.clear();

                for (submission &s : dev.in_flight)
                    vkDestroyFence(dev.device, s.fence, nullptr);
                dev.in_flight.clear();
                for (VkFence f : dev.free_fences)
                    vkDestroyFence(dev.device, f, nullptr);
                dev.free_fences.clear();

                if (dev.immediate_fence)
                    vkDestroyFence(dev.device, dev.immediate_fence, nullptr);
                if (dev.immediate_pool)
                    vkDestroyCommandPool(dev.device, dev.immediate_pool, nullptr);

                if (dev.pipeline_layout)
                    vkDestroyPipelineLayout(dev.device, dev.pipeline_layout, nullptr);
                for (VkDescriptorSetLayout layout : dev.set_layouts)
                {
                    if (layout)
                        vkDestroyDescriptorSetLayout(dev.device, layout, nullptr);
                }

                vkDestroyDevice(dev.device, nullptr);
            }

            if (dev.instance)
            {
                if (dev.messenger)
                {
                    auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                        vkGetInstanceProcAddr(dev.instance, "vkDestroyDebugUtilsMessengerEXT"));
                    if (destroy)
                        destroy(dev.instance, dev.messenger, nullptr);
                }
                vkDestroyInstance(dev.instance, nullptr);
            }

            dev.device = VK_NULL_HANDLE;
            dev.instance = VK_NULL_HANDLE;
        }
    } // namespace

    // -------------------------------------------------------------------------
    // Submission tracking
    // -------------------------------------------------------------------------

    namespace
    {
        VkFence acquire_fence(device_state &dev) noexcept
        {
            if (!dev.free_fences.empty())
            {
                VkFence f = dev.free_fences.back();
                dev.free_fences.pop_back();
                vkResetFences(dev.device, 1, &f);
                return f;
            }
            VkFenceCreateInfo info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            VkFence f = VK_NULL_HANDLE;
            if (vkCreateFence(dev.device, &info, nullptr, &f) != VK_SUCCESS)
                return VK_NULL_HANDLE;
            return f;
        }

        void collect_garbage(device_state &dev) noexcept
        {
            std::erase_if(dev.garbage, [&](deferred_release &g) {
                if (g.serial > dev.completed_serial)
                    return false;
                g.release();
                return true;
            });
        }
    } // namespace

    std::uint64_t submit_batch(device_state &dev, std::span<const VkCommandBuffer> commands,
                               std::span<const VkSemaphore> waits, std::span<const VkPipelineStageFlags> wait_stages,
                               std::span<const VkSemaphore> signals) noexcept
    {
        VkFence fence = acquire_fence(dev);
        if (!fence)
            return 0;

        VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        info.waitSemaphoreCount = static_cast<std::uint32_t>(waits.size());
        info.pWaitSemaphores = waits.data();
        info.pWaitDstStageMask = wait_stages.data();
        info.commandBufferCount = static_cast<std::uint32_t>(commands.size());
        info.pCommandBuffers = commands.data();
        info.signalSemaphoreCount = static_cast<std::uint32_t>(signals.size());
        info.pSignalSemaphores = signals.data();

        const VkResult result = vkQueueSubmit(dev.queue, 1, &info, fence);
        if (result != VK_SUCCESS)
        {
            report("submit: vkQueueSubmit failed (%s)", result_string(result));
            dev.free_fences.push_back(fence);
            return 0;
        }

        const std::uint64_t serial = ++dev.last_serial;
        dev.in_flight.push_back({fence, serial});
        poll_submissions(dev);
        return serial;
    }

    void poll_submissions(device_state &dev) noexcept
    {
        std::size_t retired = 0;
        for (const submission &s : dev.in_flight)
        {
            if (vkGetFenceStatus(dev.device, s.fence) != VK_SUCCESS)
                break; // Fences on one queue signal in submission order.
            dev.completed_serial = s.serial;
            dev.free_fences.push_back(s.fence);
            ++retired;
        }
        dev.in_flight.erase(dev.in_flight.begin(), dev.in_flight.begin() + static_cast<std::ptrdiff_t>(retired));
        collect_garbage(dev);
    }

    void wait_for_serial(device_state &dev, std::uint64_t serial) noexcept
    {
        if (serial == 0 || serial <= dev.completed_serial)
            return;

        std::vector<VkFence> fences;
        for (const submission &s : dev.in_flight)
        {
            if (s.serial <= serial)
                fences.push_back(s.fence);
        }
        if (!fences.empty())
            vkWaitForFences(dev.device, static_cast<std::uint32_t>(fences.size()), fences.data(), VK_TRUE, UINT64_MAX);
        poll_submissions(dev);
    }

    void wait_all(device_state &dev) noexcept
    {
        vkDeviceWaitIdle(dev.device);
        poll_submissions(dev);
        // Everything has completed even if a fence poll raced; retire whatever is left.
        for (const submission &s : dev.in_flight)
        {
            dev.completed_serial = s.serial;
            dev.free_fences.push_back(s.fence);
        }
        dev.in_flight.clear();
        collect_garbage(dev);
    }

    void defer_release(device_state &dev, std::function<void()> release)
    {
        poll_submissions(dev);
        if (dev.in_flight.empty())
        {
            release();
            return;
        }
        dev.garbage.push_back({dev.last_serial, std::move(release)});
    }

    VkCommandBuffer begin_immediate(device_state &dev) noexcept
    {
        if (vkResetCommandPool(dev.device, dev.immediate_pool, 0) != VK_SUCCESS)
            return VK_NULL_HANDLE;

        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(dev.immediate_cmd, &begin) != VK_SUCCESS)
            return VK_NULL_HANDLE;

        // Order against whatever earlier submissions are still touching the resources we are about to write.
        full_barrier(dev.immediate_cmd);
        return dev.immediate_cmd;
    }

    bool end_immediate(device_state &dev) noexcept
    {
        full_barrier(dev.immediate_cmd);
        if (vkEndCommandBuffer(dev.immediate_cmd) != VK_SUCCESS)
            return false;

        vkResetFences(dev.device, 1, &dev.immediate_fence);

        VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        info.commandBufferCount = 1;
        info.pCommandBuffers = &dev.immediate_cmd;
        const VkResult result = vkQueueSubmit(dev.queue, 1, &info, dev.immediate_fence);
        if (result != VK_SUCCESS)
        {
            report("immediate submit: vkQueueSubmit failed (%s)", result_string(result));
            return false;
        }

        vkWaitForFences(dev.device, 1, &dev.immediate_fence, VK_TRUE, UINT64_MAX);
        poll_submissions(dev); // Earlier submissions are complete too (fences signal in submission order).
        return true;
    }

    void full_barrier(VkCommandBuffer cmd) noexcept
    {
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1,
                             &barrier, 0, nullptr, 0, nullptr);
    }

} // namespace catalyst::rendering::detail::vulkan

// -----------------------------------------------------------------------------
// Backend contract: device
// -----------------------------------------------------------------------------

namespace catalyst::rendering::detail
{

    using namespace vulkan;

    resource_id create_device(const device_desc &desc)
    {
        device_state dev;
        dev.desc = desc;
        dev.application_name = copy_name(desc.application_name);
        if (dev.application_name.empty())
            dev.application_name = "Catalyst";
        dev.desc.application_name = nullptr;

        adapter_candidate adapter;
        const bool ok = create_instance(dev) && pick_adapter(dev, adapter) && create_logical_device(dev, adapter) &&
                        create_pipeline_layout(dev) && create_immediate_objects(dev);
        if (!ok)
        {
            destroy_device_objects(dev);
            return 0;
        }

        const resource_id id = allocate_id();
        auto [it, inserted] = reg().devices.emplace(id, std::move(dev));
        device_state &stored = it->second;
        stored.desc.application_name = stored.application_name.c_str();
        set_debug_name(stored, VK_OBJECT_TYPE_DEVICE, reinterpret_cast<std::uint64_t>(stored.device),
                       stored.application_name);
        return id;
    }

    void destroy_device(resource_id id) noexcept
    {
        device_state *dev = find_device(id);
        if (!dev)
            return;

        wait_all(*dev);

        registry &r = reg();
        release_owned(r.command_lists, id, [&](command_list_state &cl) { release_command_list_objects(*dev, cl); });
        release_owned(r.swapchains, id, [&](swapchain_state &sc) { release_swapchain_objects(*dev, sc); });
        release_owned(r.pipelines, id, [&](pipeline_state &p) { release_pipeline_objects(*dev, p); });
        release_owned(r.samplers, id, [&](sampler_state &s) { release_sampler_objects(*dev, s); });
        release_owned(r.textures, id, [&](texture_state &t) { release_texture_objects(*dev, t); });
        release_owned(r.shaders, id, [&](shader_state &s) { release_shader_objects(*dev, s); });
        release_owned(r.buffers, id, [&](buffer_state &b) { release_buffer_objects(*dev, b); });

        destroy_device_objects(*dev);
        r.devices.erase(id);
    }

    bool is_device_valid(resource_id id) noexcept
    {
        return find_device(id) != nullptr;
    }

    device_info get_device_info(resource_id id) noexcept
    {
        device_info info;
        const device_state *dev = find_device(id);
        if (!dev)
            return info;
        info.backend = backend_kind::vulkan;
        info.adapter_name = dev->adapter_name.c_str();
        info.dedicated_video_memory_bytes = dev->dedicated_video_memory;
        return info;
    }

    void wait_idle(resource_id id) noexcept
    {
        if (device_state *dev = find_device(id))
            wait_all(*dev);
    }

} // namespace catalyst::rendering::detail
