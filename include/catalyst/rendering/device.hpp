/**
 * SPDX-License-Identifier: CDDL-1.0
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The rendering device: the root object that owns every other GPU resource (buffers, shaders, textures, pipelines,
 * swapchains, command lists) and represents one logical connection to a graphics adapter.
 */

#pragma once

#include <catalyst/rendering/types.hpp>

#include <cstdint>

namespace catalyst::rendering
{

    /**
     * @struct device_desc
     * @brief Creation parameters for a rendering device.
     */
    struct device_desc
    {
        /** Reported to the driver (e.g. `VkApplicationInfo::pApplicationName`). */
        const char *application_name = "Catalyst";
        /** Enable API validation / debug layers where the backend supports them. Costs performance; use in debug builds. */
        bool enable_validation = false;
        /** Prefer a discrete GPU over an integrated one when several adapters are present. */
        bool prefer_discrete_adapter = true;
    };

    /**
     * @struct device_info
     * @brief Read-only facts about a created device.
     */
    struct device_info
    {
        backend_kind backend = backend_kind::null;
        /** Human-readable adapter name; owned by the backend and valid until the device is destroyed. */
        const char *adapter_name = "";
        std::uint64_t dedicated_video_memory_bytes = 0;
    };

    struct device_tag
    {
    };

    /**
     * @brief Handle to a rendering device. See `create_device`.
     */
    using device = resource_handle<device_tag>;

    /**
     * @brief Creates a device on the compiled-in backend. Returns an invalid handle if no suitable adapter is found.
     */
    [[nodiscard]] device create_device(const device_desc &desc = {});

    /**
     * @brief Destroys the device and every resource created from it, then resets `d` to an invalid handle.
     * Waits for the GPU to go idle first.
     */
    void destroy_device(device &d) noexcept;

    [[nodiscard]] bool is_valid(const device &d) noexcept;

    [[nodiscard]] device_info get_device_info(const device &d) noexcept;

    /**
     * @brief Blocks until all submitted work on every queue of the device has completed.
     */
    void wait_idle(const device &d) noexcept;

} // namespace catalyst::rendering
