/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The entity pool, hierarchy and transform propagation declared in world.hpp.
 */

#include <catalyst/scene/world.hpp>

#include <algorithm>
#include <utility>

namespace catalyst::scene
{

    namespace
    {
        void erase_handle(std::vector<entity> &list, entity e) noexcept
        {
            const auto it = std::find(list.begin(), list.end(), e);
            if (it != list.end())
                list.erase(it);
        }
    } // namespace

    world::world() = default;

    world::~world() = default;

    world::world(world &&) noexcept = default;

    world &world::operator=(world &&) noexcept = default;

    // ---- lookup ---------------------------------------------------------------------------------

    const world::entity_data *world::find(entity e) const noexcept
    {
        if (e.index >= entities_.size())
            return nullptr;
        const entity_data &d = entities_[e.index];
        if (!d.alive || d.generation != e.generation)
            return nullptr;
        return &d;
    }

    world::entity_data *world::find(entity e) noexcept
    {
        return const_cast<entity_data *>(std::as_const(*this).find(e));
    }

    bool world::is_valid(entity e) const noexcept
    {
        return find(e) != nullptr;
    }

    std::size_t world::entity_count() const noexcept
    {
        return live_count_;
    }

    // ---- entities -------------------------------------------------------------------------------

    entity world::create()
    {
        std::uint32_t index;
        if (!free_slots_.empty())
        {
            index = free_slots_.back();
            free_slots_.pop_back();
        }
        else
        {
            index = static_cast<std::uint32_t>(entities_.size());
            entities_.emplace_back();
        }

        entity_data &d = entities_[index];
        // The generation was bumped when the slot was retired, so a handle from the previous
        // occupant already mismatches; nothing to do here but reset the rest.
        d.alive = true;
        d.dirty = true;
        d.parent = null_entity;
        d.children.clear();
        d.name.clear();
        d.local = transform{};
        d.world = math::mat4f::identity();

        const entity e{index, d.generation};
        roots_.push_back(e);
        ++live_count_;
        return e;
    }

    entity world::create(std::string_view name)
    {
        const entity e = create();
        entities_[e.index].name.assign(name);
        return e;
    }

    entity world::create_child(entity parent)
    {
        const entity e = create();
        set_parent(e, parent);
        return e;
    }

    void world::destroy(entity e)
    {
        if (!is_valid(e))
            return;
        detach(e);
        destroy_recursive(e);
    }

    void world::destroy_recursive(entity e)
    {
        entity_data *d = find(e);
        if (d == nullptr)
            return;

        // Take the list first: destroying a child does not touch the parent's list (only `detach`
        // does, and children are not detached one by one), but moving it out makes that
        // independence explicit rather than relied upon.
        std::vector<entity> children = std::move(d->children);
        for (const entity child : children)
            destroy_recursive(child);

        erase_components(e);

        d = &entities_[e.index];
        d->alive = false;
        d->children.clear();
        d->name.clear();
        d->parent = null_entity;
        ++d->generation;
        free_slots_.push_back(e.index);
        --live_count_;
    }

    void world::erase_components(entity e) noexcept
    {
        for (pool_entry &entry : pools_)
            entry.pool->erase(e);
    }

    void world::clear() noexcept
    {
        for (std::uint32_t index = 0; index < entities_.size(); ++index)
        {
            entity_data &d = entities_[index];
            if (!d.alive)
                continue;
            d.alive = false;
            d.children.clear();
            d.name.clear();
            d.parent = null_entity;
            ++d.generation;
            free_slots_.push_back(index);
        }
        for (pool_entry &entry : pools_)
            entry.pool->clear();
        roots_.clear();
        live_count_ = 0;
    }

    // ---- names ----------------------------------------------------------------------------------

    void world::set_name(entity e, std::string_view name)
    {
        if (entity_data *d = find(e))
            d->name.assign(name);
    }

    std::string_view world::name_of(entity e) const noexcept
    {
        const entity_data *d = find(e);
        return (d == nullptr) ? std::string_view{empty_name_} : std::string_view{d->name};
    }

    // ---- hierarchy ------------------------------------------------------------------------------

    bool world::is_descendant_of(entity candidate, entity ancestor) const noexcept
    {
        for (entity current = candidate; !is_null(current);)
        {
            if (current == ancestor)
                return true;
            const entity_data *d = find(current);
            if (d == nullptr)
                return false;
            current = d->parent;
        }
        return false;
    }

    void world::detach(entity e) noexcept
    {
        entity_data *d = find(e);
        if (d == nullptr)
            return;

        if (is_null(d->parent))
        {
            erase_handle(roots_, e);
        }
        else if (entity_data *p = find(d->parent))
        {
            erase_handle(p->children, e);
        }
        d->parent = null_entity;
    }

    void world::set_parent(entity child, entity parent)
    {
        entity_data *c = find(child);
        if (c == nullptr)
            return;
        if (!is_null(parent) && !is_valid(parent))
            return;
        if (c->parent == parent)
            return;
        // Parenting an entity under its own descendant (or itself) would cut the subtree loose from
        // every root, and update_transforms would never reach it again.
        if (is_descendant_of(parent, child))
            return;

        detach(child);
        if (is_null(parent))
        {
            roots_.push_back(child);
        }
        else
        {
            entities_[parent.index].children.push_back(child);
            c->parent = parent;
        }
        c->dirty = true;
    }

    entity world::parent_of(entity e) const noexcept
    {
        const entity_data *d = find(e);
        return (d == nullptr) ? null_entity : d->parent;
    }

    std::span<const entity> world::children_of(entity e) const noexcept
    {
        const entity_data *d = find(e);
        return (d == nullptr) ? std::span<const entity>{} : std::span<const entity>{d->children};
    }

    std::size_t world::child_count(entity e) const noexcept
    {
        const entity_data *d = find(e);
        return (d == nullptr) ? 0u : d->children.size();
    }

    std::span<const entity> world::roots() const noexcept
    {
        return roots_;
    }

    // ---- transforms -----------------------------------------------------------------------------

    const transform &world::local_of(entity e) const noexcept
    {
        // A separate default from the write scratch: a write through mutable_local(invalid) must
        // not be readable back through local_of(invalid), or "discarded" would be a lie.
        static const transform k_default{};
        const entity_data *d = find(e);
        return (d == nullptr) ? k_default : d->local;
    }

    transform &world::mutable_local(entity e) noexcept
    {
        entity_data *d = find(e);
        if (d == nullptr)
        {
            scratch_local_ = transform{};
            return scratch_local_;
        }
        d->dirty = true;
        return d->local;
    }

    void world::set_local(entity e, const transform &t) noexcept
    {
        if (entity_data *d = find(e))
        {
            d->local = t;
            d->dirty = true;
        }
    }

    const math::mat4f &world::world_of(entity e) const noexcept
    {
        const entity_data *d = find(e);
        return (d == nullptr) ? identity_ : d->world;
    }

    bool world::is_transform_dirty(entity e) const noexcept
    {
        const entity_data *d = find(e);
        return (d != nullptr) && d->dirty;
    }

    void world::update_transforms() noexcept
    {
        // An explicit stack rather than recursion: a hierarchy is as deep as a level designer makes
        // it, and the stack is kept between calls so a steady-state frame allocates nothing.
        update_stack_.clear();
        for (const entity root : roots_)
            update_stack_.emplace_back(root, false);

        while (!update_stack_.empty())
        {
            const auto [e, parent_changed] = update_stack_.back();
            update_stack_.pop_back();

            entity_data &d = entities_[e.index];
            const bool changed = d.dirty || parent_changed;
            if (changed)
            {
                // The parent was popped before its children were pushed, so its world matrix is
                // already this frame's.
                const math::mat4f local = d.local.to_matrix();
                d.world = is_null(d.parent) ? local : entities_[d.parent.index].world * local;
                d.dirty = false;
            }

            for (const entity child : d.children)
                update_stack_.emplace_back(child, changed);
        }
    }

} // namespace catalyst::scene
