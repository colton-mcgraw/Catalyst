/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The world: owner of entities, their hierarchy, their transforms and their components.
 */

#pragma once

#include <catalyst/math/matrix.hpp>
#include <catalyst/scene/entity.hpp>
#include <catalyst/scene/transform.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

namespace catalyst::scene
{

    /**
     * @concept component
     * @brief Anything a `world` can attach to an entity.
     * @details No base class, no registration, no virtuals: a component is a movable object type,
     * which is the same bar `resource::registry` sets. `transform` and `entity` are excluded on
     * purpose. Every entity already has a transform, and a second one added as a component would be
     * silently ignored by `update_transforms`; an entity stored as a component is almost always a
     * mistake for a field on a real component.
     */
    template <typename T>
    concept component = std::is_object_v<T> && std::movable<T> && !std::is_same_v<std::remove_cvref_t<T>, transform> &&
                        !std::is_same_v<std::remove_cvref_t<T>, entity>;

    namespace detail
    {
        /** @brief The sparse-set sentinel for "this entity has no component of this type". */
        inline constexpr std::uint32_t no_slot = 0xFFFF'FFFFu;

        /**
         * @class pool_base
         * @brief The type-erased face of a component pool, so a world can hold pools of every type
         * in one list and destroy an entity's components without knowing what they are.
         */
        class pool_base
        {
        public:
            virtual ~pool_base() = default;

            /** @brief Removes the entity's component if it has one. */
            virtual bool erase(entity e) noexcept = 0;
            /** @brief Removes every component. */
            virtual void clear() noexcept = 0;
            /** @brief How many entities have a component in this pool. */
            [[nodiscard]] virtual std::size_t size() const noexcept = 0;
        };

        /**
         * @class pool
         * @brief A sparse set of one component type, indexed by entity slot.
         * @details Dense arrays of entities and values keep iteration contiguous, which is what
         * `world::each` is for; the sparse array maps an entity's slot index to its dense position
         * so lookup is one indirection. Removal swaps the last element into the hole, so iteration
         * order is not insertion order and is not stable across removals -- callers that need order
         * sort the extracted result, not the pool.
         *
         * The dense entity is stored with its generation, so a stale handle whose slot has since
         * been reused misses here even if it slipped past the world's validity check.
         */
        template <component T>
        class pool final : public pool_base
        {
        public:
            /** @brief Adds a component, or overwrites the one the entity already has. */
            T &insert(entity e, T value)
            {
                if (T *existing = find(e))
                {
                    *existing = std::move(value);
                    return *existing;
                }
                if (e.index >= sparse_.size())
                    sparse_.resize(static_cast<std::size_t>(e.index) + 1u, no_slot);
                sparse_[e.index] = static_cast<std::uint32_t>(entities_.size());
                entities_.push_back(e);
                values_.push_back(std::move(value));
                return values_.back();
            }

            /** @brief The entity's component, or null. */
            [[nodiscard]] T *find(entity e) noexcept
            {
                const std::uint32_t dense = dense_index(e);
                return (dense == no_slot) ? nullptr : &values_[dense];
            }

            /** @brief The entity's component, or null. */
            [[nodiscard]] const T *find(entity e) const noexcept
            {
                const std::uint32_t dense = dense_index(e);
                return (dense == no_slot) ? nullptr : &values_[dense];
            }

            bool erase(entity e) noexcept override
            {
                const std::uint32_t dense = dense_index(e);
                if (dense == no_slot)
                    return false;

                const std::uint32_t last = static_cast<std::uint32_t>(entities_.size() - 1u);
                if (dense != last)
                {
                    entities_[dense] = entities_[last];
                    values_[dense] = std::move(values_[last]);
                    sparse_[entities_[dense].index] = dense;
                }
                entities_.pop_back();
                values_.pop_back();
                sparse_[e.index] = no_slot;
                return true;
            }

            void clear() noexcept override
            {
                entities_.clear();
                values_.clear();
                sparse_.assign(sparse_.size(), no_slot);
            }

            [[nodiscard]] std::size_t size() const noexcept override { return entities_.size(); }

            /** @brief The entities with a component here, in dense order. */
            [[nodiscard]] std::span<const entity> entities() const noexcept { return entities_; }
            /** @brief The components, in the same order as `entities()`. */
            [[nodiscard]] std::span<T> values() noexcept { return values_; }
            /** @brief The components, in the same order as `entities()`. */
            [[nodiscard]] std::span<const T> values() const noexcept { return values_; }

            /** @brief The value handed back for writes against an invalid entity. See `world::add`. */
            [[nodiscard]] T &scratch() noexcept { return scratch_; }

        private:
            [[nodiscard]] std::uint32_t dense_index(entity e) const noexcept
            {
                if (e.index >= sparse_.size())
                    return no_slot;
                const std::uint32_t dense = sparse_[e.index];
                if (dense == no_slot || !(entities_[dense] == e))
                    return no_slot;
                return dense;
            }

            std::vector<std::uint32_t> sparse_;
            std::vector<entity> entities_;
            std::vector<T> values_;
            T scratch_{};
        };
    } // namespace detail

    /**
     * @class world
     * @brief Owns a set of entities, their parent/child hierarchy, their transforms and their components.
     * @details The shape is `ui::tree` extended with components: entities live in a slot pool, handles
     * are index plus generation, and accessors given an invalid handle degrade to a no-op rather than
     * crashing. Read accessors return a shared default and write accessors return a scratch slot
     * whose contents are discarded.
     *
     * Every entity has a local `transform` and a cached world matrix. Editing the local transform or
     * reparenting marks the entity dirty; `update_transforms` recomputes dirty subtrees top-down and
     * is the only place world matrices change, so `world_of` is a plain read and extraction can run
     * against a `const world &`.
     *
     * Components are anything satisfying `component`, stored per type in a sparse set. There is no
     * registration step: the first `add<T>` creates the pool.
     *
     * A world is owned by one thread. Nothing here locks, and `each` hands out references into pool
     * storage that any insertion into the same pool may move.
     */
    class world
    {
    public:
        /** @brief Constructs an empty world. */
        world();

        /** @brief Destroys the world and every entity and component in it. */
        ~world();

        world(const world &) = delete;
        world &operator=(const world &) = delete;

        /** @brief Moves a world. Handles remain valid against the moved-to world. */
        world(world &&) noexcept;

        /** @brief Move-assigns a world, destroying anything this world already owned. */
        world &operator=(world &&) noexcept;

        // ---- entities ---------------------------------------------------------------------------

        /**
         * @brief Creates a parentless entity with an identity transform and no components.
         * @return A handle to the new entity.
         */
        [[nodiscard]] entity create();

        /**
         * @brief Creates a parentless entity with a name.
         * @param name The name, copied. Names are for tooling and lookup by humans; they need not be unique.
         * @return A handle to the new entity.
         */
        [[nodiscard]] entity create(std::string_view name);

        /**
         * @brief Creates an entity as the last child of `parent`.
         * @param parent The parent. If it is invalid the entity is left parentless.
         * @return A handle to the new entity.
         */
        [[nodiscard]] entity create_child(entity parent);

        /**
         * @brief Destroys an entity, its components and its whole subtree.
         * @param e The entity to destroy. Invalid handles are ignored.
         */
        void destroy(entity e);

        /**
         * @brief Reports whether a handle refers to a live entity in this world.
         * @return True when the slot is occupied and the generation matches.
         */
        [[nodiscard]] bool is_valid(entity e) const noexcept;

        /** @brief The number of live entities. */
        [[nodiscard]] std::size_t entity_count() const noexcept;

        /**
         * @brief Destroys every entity and component.
         * @details Slots are retired rather than freed, so handles from before the clear stay invalid
         * instead of aliasing the entities created after it.
         */
        void clear() noexcept;

        // ---- names ------------------------------------------------------------------------------

        /** @brief Sets an entity's name. Ignored for an invalid entity. */
        void set_name(entity e, std::string_view name);

        /** @brief An entity's name, or an empty view for an unnamed or invalid entity. */
        [[nodiscard]] std::string_view name_of(entity e) const noexcept;

        // ---- hierarchy --------------------------------------------------------------------------

        /**
         * @brief Reparents an entity, keeping its *local* transform.
         * @details The world transform therefore changes, which is what a spawner attaching a
         * projectile to a muzzle wants. A world-preserving reparent is a later addition, not a
         * different default. The request is ignored when it would create a cycle, when `child` is
         * invalid, or when `parent` is non-null and invalid.
         * @param child The entity to move.
         * @param parent The new parent, or `null_entity` to make it a root.
         */
        void set_parent(entity child, entity parent);

        /** @brief An entity's parent, or `null_entity` for a root or invalid entity. */
        [[nodiscard]] entity parent_of(entity e) const noexcept;

        /**
         * @brief An entity's children in order.
         * @return A view valid until the hierarchy changes. Empty for an invalid entity.
         */
        [[nodiscard]] std::span<const entity> children_of(entity e) const noexcept;

        /** @brief How many children an entity has. Zero for an invalid entity. */
        [[nodiscard]] std::size_t child_count(entity e) const noexcept;

        /**
         * @brief Every parentless entity, in creation order.
         * @return A view valid until the hierarchy changes.
         */
        [[nodiscard]] std::span<const entity> roots() const noexcept;

        // ---- transforms -------------------------------------------------------------------------

        /** @brief An entity's local transform for reading. A default transform for an invalid entity. */
        [[nodiscard]] const transform &local_of(entity e) const noexcept;

        /**
         * @brief An entity's local transform for writing. Marks the entity dirty.
         * @details Every call marks the entity dirty whether or not anything changes, so prefer
         * `local_of` when only reading.
         * @return The transform, or a scratch transform that is discarded for an invalid entity.
         */
        [[nodiscard]] transform &mutable_local(entity e) noexcept;

        /** @brief Replaces an entity's local transform and marks it dirty. */
        void set_local(entity e, const transform &t) noexcept;

        /**
         * @brief An entity's world matrix as of the last `update_transforms`.
         * @details Reads the cache; it does not recompute. Call `update_transforms` after editing
         * transforms and before reading this, or the value is from the previous frame. The identity
         * for an invalid entity.
         */
        [[nodiscard]] const math::mat4f &world_of(entity e) const noexcept;

        /**
         * @brief Recomputes the world matrix of every entity whose local transform changed, or
         * whose ancestor's did, since the last call.
         * @details Top-down over the hierarchy, so each dirty subtree costs one matrix multiply per
         * entity and a clean subtree costs one flag test per entity. Clears the dirty flags.
         */
        void update_transforms() noexcept;

        /**
         * @brief Whether an entity's local transform was edited, or it was reparented, since the
         * last `update_transforms`.
         * @details Reports the entity's own flag only; a clean child of a dirty parent reads false
         * even though its world matrix will change.
         */
        [[nodiscard]] bool is_transform_dirty(entity e) const noexcept;

        // ---- components -------------------------------------------------------------------------

        /**
         * @brief Attaches a component to an entity, replacing any it already has of that type.
         * @tparam T The component type. The pool is created on first use.
         * @param e The entity.
         * @param value The component to attach.
         * @return The stored component, or a scratch value that is discarded for an invalid entity.
         */
        template <component T>
        T &add(entity e, T value = T{})
        {
            detail::pool<T> &p = pool_for<T>();
            if (!is_valid(e))
            {
                p.scratch() = T{};
                return p.scratch();
            }
            return p.insert(e, std::move(value));
        }

        /** @brief The entity's component of type `T`, or null if it has none or is invalid. */
        template <component T>
        [[nodiscard]] T *get(entity e) noexcept
        {
            detail::pool<T> *p = find_pool<T>();
            return (p == nullptr) ? nullptr : p->find(e);
        }

        /** @brief The entity's component of type `T`, or null if it has none or is invalid. */
        template <component T>
        [[nodiscard]] const T *get(entity e) const noexcept
        {
            const detail::pool<T> *p = find_pool<T>();
            return (p == nullptr) ? nullptr : p->find(e);
        }

        /** @brief Whether the entity has a component of type `T`. */
        template <component T>
        [[nodiscard]] bool has(entity e) const noexcept
        {
            return get<T>(e) != nullptr;
        }

        /**
         * @brief Detaches and destroys the entity's component of type `T`.
         * @return True when there was one to remove.
         */
        template <component T>
        bool remove(entity e) noexcept
        {
            detail::pool<T> *p = find_pool<T>();
            return (p != nullptr) && p->erase(e);
        }

        /** @brief How many entities have a component of type `T`. */
        template <component T>
        [[nodiscard]] std::size_t count() const noexcept
        {
            const detail::pool<T> *p = find_pool<T>();
            return (p == nullptr) ? 0u : p->size();
        }

        /**
         * @brief Every entity with a component of type `T`, in pool order.
         * @return A view valid until a component of type `T` is added or removed.
         */
        template <component T>
        [[nodiscard]] std::span<const entity> entities_with() const noexcept
        {
            const detail::pool<T> *p = find_pool<T>();
            return (p == nullptr) ? std::span<const entity>{} : p->entities();
        }

        /**
         * @brief Visits every component of type `T` with its entity.
         * @details Pool order, which is not creation order. The visitor may edit the component and
         * may add or remove components of *other* types; adding or removing a `T` during the visit
         * is not supported, because removal swaps the last element into the current slot.
         * @param fn Called as `fn(entity, T &)`.
         */
        template <component T, std::invocable<entity, T &> F>
        void each(F &&fn)
        {
            detail::pool<T> *p = find_pool<T>();
            if (p == nullptr)
                return;
            const std::span<const entity> entities = p->entities();
            const std::span<T> values = p->values();
            for (std::size_t i = 0; i < entities.size(); ++i)
                fn(entities[i], values[i]);
        }

        /**
         * @brief Visits every component of type `T` with its entity, read-only.
         * @param fn Called as `fn(entity, const T &)`.
         */
        template <component T, std::invocable<entity, const T &> F>
        void each(F &&fn) const
        {
            const detail::pool<T> *p = find_pool<T>();
            if (p == nullptr)
                return;
            const std::span<const entity> entities = p->entities();
            const std::span<const T> values = p->values();
            for (std::size_t i = 0; i < entities.size(); ++i)
                fn(entities[i], values[i]);
        }

    private:
        struct entity_data
        {
            std::uint32_t generation = 0u;
            bool alive = false;
            bool dirty = true;
            entity parent = null_entity;
            std::vector<entity> children;
            std::string name;
            transform local{};
            math::mat4f world = math::mat4f::identity();
        };

        struct pool_entry
        {
            std::type_index type;
            std::unique_ptr<detail::pool_base> pool;
        };

        [[nodiscard]] const entity_data *find(entity e) const noexcept;
        [[nodiscard]] entity_data *find(entity e) noexcept;
        [[nodiscard]] bool is_descendant_of(entity candidate, entity ancestor) const noexcept;
        void detach(entity e) noexcept;
        void destroy_recursive(entity e);
        void erase_components(entity e) noexcept;

        template <component T>
        [[nodiscard]] detail::pool<T> &pool_for()
        {
            if (detail::pool<T> *p = find_pool<T>())
                return *p;
            pools_.push_back(pool_entry{std::type_index{typeid(T)}, std::make_unique<detail::pool<T>>()});
            return static_cast<detail::pool<T> &>(*pools_.back().pool);
        }

        template <component T>
        [[nodiscard]] detail::pool<T> *find_pool() noexcept
        {
            // A linear scan: a scene has a handful of component types, and a hash lookup would cost
            // more than comparing that many type_index values.
            const std::type_index key{typeid(T)};
            for (pool_entry &entry : pools_)
                if (entry.type == key)
                    return static_cast<detail::pool<T> *>(entry.pool.get());
            return nullptr;
        }

        template <component T>
        [[nodiscard]] const detail::pool<T> *find_pool() const noexcept
        {
            const std::type_index key{typeid(T)};
            for (const pool_entry &entry : pools_)
                if (entry.type == key)
                    return static_cast<const detail::pool<T> *>(entry.pool.get());
            return nullptr;
        }

        std::vector<entity_data> entities_;
        std::vector<std::uint32_t> free_slots_;
        std::vector<entity> roots_;
        std::vector<pool_entry> pools_;
        std::vector<std::pair<entity, bool>> update_stack_;
        std::size_t live_count_ = 0;

        transform scratch_local_{};
        std::string empty_name_;
        math::mat4f identity_ = math::mat4f::identity();
    };

} // namespace catalyst::scene
