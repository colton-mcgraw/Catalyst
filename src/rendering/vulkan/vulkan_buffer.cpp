/**
 * SPDX-License-Identifier: CDDL-1.0
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Buffer resources of the Vulkan backend. Host-visible buffers stay persistently mapped so `write_buffer` and
 * `read_buffer` are memcpys; GPU-only buffers are written through a staging copy on the immediate command buffer.
 */

#include "vulkan_backend.hpp"
#include "vulkan_convert.hpp"

#include <cstring>

namespace catalyst::rendering::detail::vulkan
{

    void release_buffer_objects(device_state &dev, buffer_state &b) noexcept
    {
        if (b.buffer)
            vkDestroyBuffer(dev.device, b.buffer, nullptr);
        if (b.memory)
            vkFreeMemory(dev.device, b.memory, nullptr);
        b.buffer = VK_NULL_HANDLE;
        b.memory = VK_NULL_HANDLE;
        b.mapped = nullptr;
    }

    namespace
    {
        bool upload_via_staging(device_state &dev, VkBuffer dst, VkDeviceSize offset,
                                std::span<const std::byte> data) noexcept
        {
            staging_buffer staging;
            if (!create_staging_buffer(dev, data, staging))
                return false;

            bool ok = false;
            if (VkCommandBuffer cmd = begin_immediate(dev))
            {
                VkBufferCopy region{};
                region.srcOffset = 0;
                region.dstOffset = offset;
                region.size = data.size();
                vkCmdCopyBuffer(cmd, staging.buffer, dst, 1, &region);
                ok = end_immediate(dev);
            }
            destroy_staging_buffer(dev, staging);
            return ok;
        }
    } // namespace

} // namespace catalyst::rendering::detail::vulkan

namespace catalyst::rendering::detail
{

    using namespace vulkan;

    resource_id create_buffer(resource_id device, const buffer_desc &desc, std::span<const std::byte> initial_data)
    {
        device_state *dev = find_device(device);
        if (!dev)
            return 0;

        buffer_state b;
        b.owner = device;
        b.desc = desc;
        b.desc.debug_name = nullptr;
        b.debug_name = copy_name(desc.debug_name);

        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = desc.size_bytes;
        info.usage = to_vk_buffer_usage(desc.usage);
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        const VkResult result = vkCreateBuffer(dev->device, &info, nullptr, &b.buffer);
        if (result != VK_SUCCESS)
        {
            report("create_buffer: vkCreateBuffer failed (%s)", result_string(result));
            return 0;
        }

        if (!allocate_buffer_memory(*dev, b.buffer, desc.access, b.memory, b.mapped, b.coherent))
        {
            release_buffer_objects(*dev, b);
            return 0;
        }

        if (!initial_data.empty())
        {
            if (b.mapped)
            {
                std::memcpy(b.mapped, initial_data.data(), initial_data.size());
                if (!b.coherent)
                    flush_host_writes(*dev, b.memory);
            }
            else if (!upload_via_staging(*dev, b.buffer, 0, initial_data))
            {
                release_buffer_objects(*dev, b);
                return 0;
            }
        }

        set_debug_name(*dev, VK_OBJECT_TYPE_BUFFER, handle_bits(b.buffer), b.debug_name);

        const resource_id id = allocate_id();
        reg().buffers.emplace(id, std::move(b));
        return id;
    }

    void destroy_buffer(resource_id id) noexcept
    {
        buffer_state *b = find(reg().buffers, id);
        if (!b)
            return;

        if (device_state *dev = find_device(b->owner))
        {
            const VkDevice d = dev->device;
            const VkBuffer buffer = b->buffer;
            const VkDeviceMemory memory = b->memory;
            defer_release(*dev, [d, buffer, memory] {
                vkDestroyBuffer(d, buffer, nullptr);
                vkFreeMemory(d, memory, nullptr);
            });
        }
        reg().buffers.erase(id);
    }

    bool is_buffer_valid(resource_id id) noexcept
    {
        return find(reg().buffers, id) != nullptr;
    }

    buffer_desc get_buffer_desc(resource_id id) noexcept
    {
        const buffer_state *b = find(reg().buffers, id);
        if (!b)
            return {};
        buffer_desc d = b->desc;
        d.debug_name = name_or_null(b->debug_name);
        return d;
    }

    bool write_buffer(resource_id id, std::size_t offset, std::span<const std::byte> data) noexcept
    {
        buffer_state *b = find(reg().buffers, id);
        if (!b || b->desc.access == memory_access::gpu_to_cpu)
            return false;
        if (!range_in_bounds(b->desc.size_bytes, offset, data.size()))
            return false;
        if (data.empty())
            return true;

        device_state *dev = find_device(b->owner);
        if (!dev)
            return false;

        if (b->mapped)
        {
            std::memcpy(static_cast<std::byte *>(b->mapped) + offset, data.data(), data.size());
            if (!b->coherent)
                flush_host_writes(*dev, b->memory);
            return true;
        }
        return upload_via_staging(*dev, b->buffer, offset, data);
    }

    bool read_buffer(resource_id id, std::size_t offset, std::span<std::byte> out) noexcept
    {
        buffer_state *b = find(reg().buffers, id);
        if (!b || b->desc.access != memory_access::gpu_to_cpu || !b->mapped)
            return false;
        if (!range_in_bounds(b->desc.size_bytes, offset, out.size()))
            return false;
        if (out.empty())
            return true;

        device_state *dev = find_device(b->owner);
        if (!dev)
            return false;

        // The GPU writes, the CPU reads: make sure every submission issued so far has finished.
        wait_for_serial(*dev, dev->last_serial);
        if (!b->coherent)
            invalidate_host_reads(*dev, b->memory);

        std::memcpy(out.data(), static_cast<const std::byte *>(b->mapped) + offset, out.size());
        return true;
    }

} // namespace catalyst::rendering::detail
