/**
 * SPDX-License-Identifier: CDDL-1.0
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file rendering.hpp
 * @brief Umbrella header for the Catalyst Rendering library. Including it pulls in the whole public API; individual
 * headers can be included instead to keep compile times down.
 * @details The rendering module exposes a thin, explicit, backend-agnostic layer over Vulkan, D3D12 and Metal. It is
 * organised around one `device` from which every other resource is created, opaque handles for those resources, and
 * `command_list`s that record work for `submit`. See the individual headers for details:
 *   - types.hpp      handles, formats, flag helpers, geometry structs
 *   - device.hpp     device creation and adapter queries
 *   - buffer.hpp     buffers and `structured_buffer<T>`
 *   - shader.hpp     shader modules from bytecode
 *   - texture.hpp    textures and samplers
 *   - pipeline.hpp   graphics / compute pipeline state objects
 *   - swapchain.hpp  presentable back buffers for a platform window
 *   - command.hpp    command lists, render passes and submission
 *
 * A minimal frame looks like:
 * @code
 *   using namespace catalyst::rendering;
 *   device dev = create_device();
 *   swapchain sc = create_swapchain(dev, {.window = platform::get_native_handle(w), .extent = {1280, 720}});
 *   command_list cl = create_command_list(dev);
 *
 *   texture back_buffer = acquire_next_image(sc);
 *   begin_recording(cl);
 *   const color_attachment color{.target = back_buffer, .clear = {0.1f, 0.1f, 0.1f, 1.0f}};
 *   begin_render_pass(cl, {.color_attachments = std::span{&color, 1}});
 *   // set_pipeline / set_vertex_buffer / draw ...
 *   end_render_pass(cl);
 *   end_recording(cl);
 *   submit(dev, cl);
 *   present(sc);
 * @endcode
 */

#pragma once

#include <catalyst/rendering/types.hpp>
#include <catalyst/rendering/device.hpp>
#include <catalyst/rendering/buffer.hpp>
#include <catalyst/rendering/shader.hpp>
#include <catalyst/rendering/texture.hpp>
#include <catalyst/rendering/pipeline.hpp>
#include <catalyst/rendering/swapchain.hpp>
#include <catalyst/rendering/command.hpp>

/**
 * @namespace catalyst::rendering
 * @brief All types and functions of the Catalyst Rendering library.
 */
namespace catalyst::rendering
{

    /**
     * @fn module_name
     * @brief Returns the name of the compiled-in rendering backend ("vulkan", "d3d12", "metal" or "null").
     */
    const char *module_name();

    /**
     * @fn backend
     * @brief Returns the compiled-in rendering backend as an enumerator.
     */
    [[nodiscard]] backend_kind backend() noexcept;

} // namespace catalyst::rendering
