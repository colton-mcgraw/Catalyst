/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The entity handle by which everything in a scene `world` is named.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace catalyst::scene
{

    /**
     * @struct entity
     * @brief A handle to an entity in a `world`.
     * @details The same shape as `ui::node`: `index` selects a slot in the owning world and
     * `generation` records which occupant of that slot the handle means, so a handle kept across a
     * destroy is rejected by `world::is_valid` rather than silently naming whatever was created in
     * the recycled slot. It is a value: copying it is free and it keeps nothing alive.
     */
    struct entity
    {
        /** @brief The index of the entity's slot in its owning world. */
        std::uint32_t index = 0xFFFF'FFFFu;
        /** @brief The generation of the slot occupant this handle refers to. */
        std::uint32_t generation = 0u;

        /** @brief Handles are equal when they name the same occupant of the same slot. */
        [[nodiscard]] constexpr bool operator==(const entity &other) const noexcept = default;
    };

    /** @brief The handle that refers to no entity. `world::is_valid` always rejects it. */
    inline constexpr entity null_entity{};

    /**
     * @brief Reports whether a handle is the null handle.
     * @details A non-null handle may still be invalid if the entity it named has been destroyed;
     * only `world::is_valid` can tell.
     */
    [[nodiscard]] constexpr bool is_null(entity e) noexcept
    {
        return e.index == null_entity.index;
    }

} // namespace catalyst::scene

/** @brief Lets an `entity` key an unordered container, for application-side tables keyed by entity. */
template <>
struct std::hash<catalyst::scene::entity>
{
    [[nodiscard]] std::size_t operator()(const catalyst::scene::entity &e) const noexcept
    {
        // Index in the low half, generation in the high half: distinct handles map to distinct
        // 64-bit values, so this is a perfect hash on 64-bit size_t and a plain mix on 32-bit.
        const std::uint64_t packed = (static_cast<std::uint64_t>(e.generation) << 32) | e.index;
        return std::hash<std::uint64_t>{}(packed);
    }
};
