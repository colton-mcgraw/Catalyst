#pragma once

#include <catalyst/events/tag.hpp>
#include <catalyst/events/task.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>

namespace catalyst::events::detail
{

    // ------------------------------------------------------------
    // Registry design
    // ------------------------------------------------------------
    //
    // A dispatch must find the callables for its event type and run them without
    // holding a lock, while other threads add and remove registrations. The read
    // side is the hot path, so it is built so that the whole cost is two
    // uncontended atomic increments on a per-thread cache line plus a handful of
    // loads:
    //
    //   * Every event type the bus has ever seen owns an `entry`, allocated once
    //     and never freed while the bus lives. Readers find it in a flat,
    //     open-addressed `table` of atomic entry pointers. Entries are only ever
    //     added, so an in-place insert is safe to race with a probe; the table is
    //     only replaced when it grows.
    //
    //   * Each entry publishes one immutable `slot_list` per callable kind through
    //     an atomic pointer. A change builds a new list and swaps the pointer, so a
    //     dispatch that loaded the old one keeps a consistent snapshot.
    //
    //   * Readers announce themselves by incrementing one of a few sharded
    //     counters on the bus (`read_guard`) before loading anything and
    //     decrementing it afterwards. A writer swaps a pointer and then reads all
    //     the shards: if they are all zero, no reader can still hold the old
    //     object, because any reader that incremented after the writer's swap is
    //     guaranteed (by the seq_cst total order) to load the new pointer. If some
    //     reader is active, the old object is parked in a graveyard and freed by a
    //     later writer that does find the bus quiescent, or by the bus's
    //     destructor.
    //
    // The price is that a long-running callable pins every list retired since it
    // started, not only its own, until it returns. That bounds memory by the
    // amount of add/remove churn during the callable, which is the trade RCU
    // makes too.
    //
    // Asynchronous dispatch cannot hold a guard across a suspension, so it pins
    // its lists with the intrusive reference count every list carries, inside a
    // guard, and then releases the guard. The lists then live exactly as long as
    // that coroutine needs them.

    /**
     * @enum kind
     * @brief Identifies which list a registration lives in, so that a token can
     * remove it without knowing its event type at compile time.
     */
    enum class kind
    {
        sync_listener,
        async_listener,
        sync_middleware,
        async_middleware
    };

    using sync_listener_sig = void(const void *);
    using async_listener_sig = task<void>(const void *);
    using sync_middleware_sig = void(void *, void *, void (*)(void *, void *));
    using async_middleware_sig = task<void>(void *, void *, task<void> (*)(void *, void *));

    // --------------------------------------------------------
    // Slots
    // --------------------------------------------------------

    /**
     * @struct slot_base
     * @brief The type-erased part of a registration, shared with tokens.
     */
    struct slot_base
    {
        std::size_t id;
        int priority;
        std::atomic<bool> active{true};

        slot_base(std::size_t id, int priority) noexcept : id(id), priority(priority) {}
    };

    template <typename Signature>
    struct slot;

    /**
     * @struct slot
     * @brief A registration together with a thunk that invokes the callable it holds.
     * @note Invocation is one indirect call through `thunk`: the callable itself is stored
     *       inline in the derived `slot_impl`, so there is no second allocation, no vtable
     *       and no std::function-style indirection on the dispatch path.
     * @tparam R The erased return type.
     * @tparam Args The erased arguments.
     */
    template <typename R, typename... Args>
    struct slot<R(Args...)> : slot_base
    {
        using thunk_type = R (*)(slot_base &, Args...);

        thunk_type thunk;

        slot(std::size_t id, int priority, thunk_type thunk) noexcept : slot_base(id, priority), thunk(thunk) {}

        R operator()(Args... args) { return thunk(*this, std::forward<Args>(args)...); }
    };

    template <typename F, typename Signature>
    struct slot_impl;

    template <typename F, typename R, typename... Args>
    struct slot_impl<F, R(Args...)> final : slot<R(Args...)>
    {
        F fn;

        template <typename G>
        slot_impl(std::size_t id, int priority, G &&g) : slot<R(Args...)>(id, priority, &call), fn(std::forward<G>(g))
        {
        }

        static R call(slot_base &base, Args... args)
        {
            auto &self = static_cast<slot_impl &>(base);
            // `if constexpr` rather than a plain `return`: a callable whose own return type is
            // non-void may be stored under a `void(...)` signature.
            if constexpr (std::is_void_v<R>)
                std::invoke(self.fn, std::forward<Args>(args)...);
            else
                return std::invoke(self.fn, std::forward<Args>(args)...);
        }
    };

    template <typename Signature, typename F>
    std::shared_ptr<slot<Signature>> make_slot(std::size_t id, int priority, F &&f)
    {
        return std::make_shared<slot_impl<std::decay_t<F>, Signature>>(id, priority, std::forward<F>(f));
    }

    inline bool live(const slot_base &s) noexcept
    {
        return s.active.load(std::memory_order_acquire);
    }

    // --------------------------------------------------------
    // Immutable slot lists
    // --------------------------------------------------------

    /**
     * @struct slot_list
     * @brief One published list of registrations, in dispatch order. Immutable once published.
     * @note `refs` starts at one for the reference the owning entry publishes. Asynchronous
     *       dispatch takes further references to outlive its read guard.
     */
    template <typename Signature>
    struct slot_list
    {
        using slot_type = slot<Signature>;

        mutable std::atomic<std::size_t> refs{1};
        std::vector<std::shared_ptr<slot_type>> slots;
    };

    template <typename Signature>
    void release(const slot_list<Signature> *list) noexcept
    {
        if (list && list->refs.fetch_sub(1, std::memory_order_acq_rel) == 1)
            delete list;
    }

    /**
     * @class pinned
     * @brief Owns one reference to a slot_list, or nothing.
     */
    template <typename Signature>
    class pinned
    {
    public:
        pinned() = default;

        // Must be called while a read_guard is held: that is what guarantees the list has
        // not been freed between the load and the increment.
        explicit pinned(const slot_list<Signature> *list) noexcept : list_(list)
        {
            if (list_)
                list_->refs.fetch_add(1, std::memory_order_relaxed);
        }

        pinned(pinned &&other) noexcept : list_(std::exchange(other.list_, nullptr)) {}
        pinned &operator=(pinned &&other) noexcept
        {
            if (this != &other)
            {
                release(list_);
                list_ = std::exchange(other.list_, nullptr);
            }
            return *this;
        }

        pinned(const pinned &) = delete;
        pinned &operator=(const pinned &) = delete;

        ~pinned() { release(list_); }

        const slot_list<Signature> *get() const noexcept { return list_; }

    private:
        const slot_list<Signature> *list_ = nullptr;
    };

    using sync_listener_list = slot_list<sync_listener_sig>;
    using async_listener_list = slot_list<async_listener_sig>;
    using sync_middleware_list = slot_list<sync_middleware_sig>;
    using async_middleware_list = slot_list<async_middleware_sig>;

    // --------------------------------------------------------
    // Entries and the table
    // --------------------------------------------------------

    /**
     * @struct entry
     * @brief Everything registered for one event type. Allocated once per type, freed with the bus.
     */
    struct entry
    {
        hashed_key key;

        std::atomic<const sync_middleware_list *> sync_middleware{nullptr};
        std::atomic<const sync_listener_list *> sync_listeners{nullptr};
        std::atomic<const async_middleware_list *> async_middleware{nullptr};
        std::atomic<const async_listener_list *> async_listeners{nullptr};

        explicit entry(const hashed_key &key) noexcept : key(key) {}

        template <typename Signature>
        std::atomic<const slot_list<Signature> *> &published() noexcept
        {
            if constexpr (std::is_same_v<Signature, sync_middleware_sig>)
                return sync_middleware;
            else if constexpr (std::is_same_v<Signature, sync_listener_sig>)
                return sync_listeners;
            else if constexpr (std::is_same_v<Signature, async_middleware_sig>)
                return async_middleware;
            else
                return async_listeners;
        }
    };

    /**
     * @struct table
     * @brief Open-addressed, linearly probed map from hashed_key to entry. Buckets only ever go
     * from empty to occupied, which is what makes a concurrent probe safe against an insert.
     */
    struct table
    {
        std::unique_ptr<std::atomic<entry *>[]> buckets;
        std::size_t mask;
        std::size_t count = 0;

        explicit table(std::size_t capacity) : buckets(new std::atomic<entry *>[capacity]), mask(capacity - 1)
        {
            for (std::size_t i = 0; i < capacity; ++i)
                buckets[i].store(nullptr, std::memory_order_relaxed);
        }

        entry *find(const hashed_key &key) const noexcept
        {
            for (std::size_t i = key.hash & mask;; i = (i + 1) & mask)
            {
                entry *e = buckets[i].load(std::memory_order_acquire);
                if (!e)
                    return nullptr;
                if (e->key == key)
                    return e;
            }
        }

        // Caller guarantees the key is absent and the table is below its load factor.
        void insert(entry *e) noexcept
        {
            std::size_t i = e->key.hash & mask;
            while (buckets[i].load(std::memory_order_relaxed))
                i = (i + 1) & mask;
            buckets[i].store(e, std::memory_order_release);
            ++count;
        }

        bool full() const noexcept { return (count + 1) * 2 > mask + 1; }
    };

    // --------------------------------------------------------
    // Reader guard
    // --------------------------------------------------------

    inline constexpr std::size_t reader_shards = 16;
    inline constexpr std::size_t cache_line = 64;

    struct alignas(cache_line) reader_shard
    {
        std::atomic<std::size_t> count{0};
    };

    // Each thread settles on one shard for life, so that under a steady load every
    // thread increments its own cache line rather than fighting over a single counter.
    inline std::size_t this_thread_shard() noexcept
    {
        static std::atomic<unsigned> next{0};
        static thread_local const std::size_t shard = next.fetch_add(1, std::memory_order_relaxed) % reader_shards;
        return shard;
    }

    /**
     * @struct retired
     * @brief An object unpublished while a reader may still hold it, and how to free it.
     */
    struct retired
    {
        void *object;
        void (*reclaim)(void *) noexcept;
    };

    /**
     * @struct state
     * @brief The bus's registry, shared between the bus and its tokens so that a token can tell
     * whether the bus still exists.
     * @note Readers touch `current_table` and `readers`; writers hold `mutex` and own the rest.
     */
    struct alignas(cache_line) state
    {
        std::atomic<const table *> current_table{nullptr};

        reader_shard readers[reader_shards];

        std::mutex mutex;
        std::atomic<std::size_t> next_id{0};
        std::vector<std::unique_ptr<entry>> entries;
        std::vector<retired> graveyard;

        state() = default;
        state(const state &) = delete;
        state &operator=(const state &) = delete;

        // The bus must outlive its in-flight dispatches, so nothing here is still being read.
        ~state()
        {
            for (auto &e : entries)
            {
                release(e->sync_middleware.load(std::memory_order_relaxed));
                release(e->sync_listeners.load(std::memory_order_relaxed));
                release(e->async_middleware.load(std::memory_order_relaxed));
                release(e->async_listeners.load(std::memory_order_relaxed));
            }
            for (const auto &r : graveyard)
                r.reclaim(r.object);
            delete current_table.load(std::memory_order_relaxed);
        }

        // ---- read side ----

        entry *find(const hashed_key &key) const noexcept
        {
            // seq_cst, not acquire: see the quiescence argument in the header comment.
            const table *t = current_table.load(std::memory_order_seq_cst);
            return t ? t->find(key) : nullptr;
        }

        // ---- write side: every member below assumes `mutex` is held ----

        bool quiescent() const noexcept
        {
            for (const auto &shard : readers)
                if (shard.count.load(std::memory_order_seq_cst) != 0)
                    return false;
            return true;
        }

        // Parks `r` until no dispatch can be holding it. Every mutation calls collect(), so in the
        // common case, a removal with no dispatch in flight, the object is freed before the
        // mutation returns.
        void dispose(retired r) { graveyard.push_back(r); }

        void collect()
        {
            if (graveyard.empty() || !quiescent())
                return;
            for (const auto &g : graveyard)
                g.reclaim(g.object);
            graveyard.clear();
        }

        entry &entry_for(const hashed_key &key)
        {
            const table *t = current_table.load(std::memory_order_relaxed);
            if (t)
                if (entry *e = t->find(key))
                    return *e;

            entries.push_back(std::make_unique<entry>(key));
            entry *e = entries.back().get();

            if (!t || t->full())
            {
                auto grown = std::make_unique<table>(t ? (t->mask + 1) * 2 : 16);
                for (auto &existing : entries)
                    grown->insert(existing.get());
                const table *previous = current_table.exchange(grown.release(), std::memory_order_seq_cst);
                if (previous)
                    dispose({const_cast<table *>(previous), [](void *p) noexcept { delete static_cast<table *>(p); }});
            }
            else
            {
                const_cast<table *>(t)->insert(e);
            }
            return *e;
        }

        template <typename Signature>
        static void reclaim_list(void *p) noexcept
        {
            release(static_cast<const slot_list<Signature> *>(p));
        }

        template <typename Signature>
        void publish(entry &e, const slot_list<Signature> *updated)
        {
            const auto *previous = e.published<Signature>().exchange(updated, std::memory_order_seq_cst);
            if (previous)
                dispose({const_cast<slot_list<Signature> *>(previous), &reclaim_list<Signature>});
            collect();
        }

        // Stable insert: after every entry of equal or higher priority.
        template <typename Signature>
        void add(const hashed_key &key, std::shared_ptr<slot<Signature>> s)
        {
            entry &e = entry_for(key);
            const auto *current = e.published<Signature>().load(std::memory_order_relaxed);

            auto updated = std::make_unique<slot_list<Signature>>();
            if (current)
            {
                updated->slots.reserve(current->slots.size() + 1);
                updated->slots = current->slots;
            }
            auto pos = std::ranges::find_if(updated->slots, [&](const auto &x) { return x->priority < s->priority; });
            updated->slots.insert(pos, std::move(s));

            publish<Signature>(e, updated.release());
        }

        template <typename Signature>
        void remove(const hashed_key &key, std::size_t id)
        {
            const table *t = current_table.load(std::memory_order_relaxed);
            entry *e = t ? t->find(key) : nullptr;
            if (!e)
                return;

            const auto *current = e->published<Signature>().load(std::memory_order_relaxed);
            if (!current)
                return;

            auto pos = std::ranges::find(current->slots, id, [](const auto &x) { return x->id; });
            if (pos == current->slots.end())
                return;

            (*pos)->active.store(false, std::memory_order_release);

            slot_list<Signature> *updated = nullptr;
            if (current->slots.size() > 1)
            {
                updated = new slot_list<Signature>();
                updated->slots.reserve(current->slots.size() - 1);
                updated->slots.insert(updated->slots.end(), current->slots.begin(), pos);
                updated->slots.insert(updated->slots.end(), pos + 1, current->slots.end());
            }

            publish<Signature>(*e, updated);
        }

        void remove(kind k, const hashed_key &key, std::size_t id)
        {
            switch (k)
            {
            case kind::sync_listener:
                remove<sync_listener_sig>(key, id);
                break;
            case kind::async_listener:
                remove<async_listener_sig>(key, id);
                break;
            case kind::sync_middleware:
                remove<sync_middleware_sig>(key, id);
                break;
            case kind::async_middleware:
                remove<async_middleware_sig>(key, id);
                break;
            }
        }
    };

    /**
     * @class read_guard
     * @brief Marks the calling thread as reading the registry for the guard's lifetime.
     */
    class read_guard
    {
    public:
        explicit read_guard(state &s) noexcept : count_(s.readers[this_thread_shard()].count)
        {
            count_.fetch_add(1, std::memory_order_seq_cst);
        }

        read_guard(const read_guard &) = delete;
        read_guard &operator=(const read_guard &) = delete;

        ~read_guard() { count_.fetch_sub(1, std::memory_order_seq_cst); }

    private:
        std::atomic<std::size_t> &count_;
    };

} // namespace catalyst::events::detail
