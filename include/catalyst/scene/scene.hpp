/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Umbrella header for the catalyst::scene module.
 * @details Including this header pulls in the whole scene module: the entity handle, the world
 * that owns entities and their hierarchy, the transform, camera, light and renderable types, the
 * bounding volumes, and the extraction pass that turns a world and a view into a render list.
 * Individual headers can be included instead when only part of the module is needed.
 */

#pragma once

#include <catalyst/scene/bounds.hpp>
#include <catalyst/scene/camera.hpp>
#include <catalyst/scene/entity.hpp>
#include <catalyst/scene/light.hpp>
#include <catalyst/scene/render_list.hpp>
#include <catalyst/scene/renderable.hpp>
#include <catalyst/scene/transform.hpp>
#include <catalyst/scene/world.hpp>

/**
 * @namespace catalyst::scene
 * @brief Three-dimensional scenes: a world of entities with a transform hierarchy and components,
 * and the extraction pass that produces what a renderer draws.
 * @details Applications create entities in a `world`, parent them to each other, edit their
 * `transform`s and attach components -- `camera`, `light`, `renderable`, or their own types. Each
 * frame they call `world::update_transforms`, build a `render_view` from a camera with `make_view`,
 * and call `extract` to get a `render_list`: a flat, culled, sorted list of draws and lights that
 * refers to meshes and materials by id only. The module never talks to a graphics API itself; a
 * separate renderer bridge resolves the ids and records the commands. That keeps the world and the
 * extraction testable without a device, in the same way `catalyst::ui` emits a draw list rather
 * than drawing.
 */
namespace catalyst::scene
{

    /**
     * @fn module_name
     * @brief Returns the name of this module as a string. This can be used for logging, debugging, or any situation
     * where you want to identify the module by name.
     * @return A string literal representing the name of this module.
     */
    const char *module_name();

} // namespace catalyst::scene
