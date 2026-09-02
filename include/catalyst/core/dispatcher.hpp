/**
 * @file dispatcher.hpp
 * @brief Defines the dispatcher class, which manages event subscriptions and delivers published events to the matching
 * handlers, in priority order, with support for event consumption, full reentrancy (handlers may subscribe, unsubscribe
 * and publish while a dispatch is in progress) and concurrent use from any number of threads.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include "event.hpp"
#include "subscription.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace catalyst::core
{
    namespace detail
    {
        /**
         * @struct handler_node
         * @brief The part of a registered handler that must not be duplicated: the callable itself and the flag that says
         * whether it is still subscribed.
         * @details Handler tables are copied on every structural change (see dispatcher_state), and a callable that was
         * copied along with them would be a bug rather than an optimisation - a handler holding mutable state would end up
         * with one copy per table generation, and updates to the copy an in-flight dispatch is running would be lost. The
         * node is therefore shared by every generation of the table and by the subscription handle, and only the cheap,
         * immutable bookkeeping is copied.
         */
        struct handler_node
        {
            std::atomic_bool active{true};
            std::function<bool(const event_base &)> invoke;
        };

        /** @brief One registered handler as it appears in a handler table. Copying this copies two refcounts. */
        struct handler_entry
        {
            std::size_t token = 0u;
            int priority = 0;
            std::shared_ptr<handler_node> node;
        };

        /** @brief All handlers for one event type, sorted by descending priority (stable for equal priorities). */
        using handler_list = std::vector<handler_entry>;

        /**
         * @typedef handler_list_ptr
         * @brief An immutable handler table. Once published it is never modified, so a dispatch can iterate it without
         * holding a lock and without caring what other threads do to the dispatcher meanwhile.
         */
        using handler_list_ptr = std::shared_ptr<const handler_list>;

        /**
         * @class handler_cache
         * @brief A thread's cache of the handler tables it has looked up, so that a publish on the hot path costs neither a
         * lock nor an atomic refcount.
         * @details A handler table is immutable once published, so a thread that has already resolved one for a given
         * (dispatcher, event type) can keep using it until the dispatcher tells it not to - which is what the dispatcher's
         * version counter is for. Validating a cached table is then a single relaxed-cost load of that counter and two
         * integer comparisons, against a reader-lock acquire/release plus a shared_ptr refcount pair for the uncached path.
         * On this machine that is the difference between roughly 6 ns and roughly 32 ns per publish, which is worth a cache.
         *
         * The cache is per thread and needs no synchronisation of its own. It is direct-mapped with a short probe window;
         * a miss simply falls back to looking the table up under the dispatcher's reader lock.
         *
         * @par Pinning
         * An entry is pinned for as long as a dispatch is iterating it, because handlers may publish, and a nested publish
         * of another event type could otherwise evict the entry the outer dispatch is walking and free the table underneath
         * it. Pinning is a plain thread-local counter, not an atomic. A refill that finds every candidate slot pinned
         * simply does not cache: the caller keeps its own reference to the table for the duration instead.
         */
        class handler_cache
        {
        public:
            struct entry
            {
                std::uint64_t state_id = 0u; ///< 0 is never a live dispatcher, so a zeroed entry never matches
                event_type_id type_id = 0u;
                std::uint64_t version = 0u;
                std::uint32_t pins = 0u;
                handler_list_ptr list; ///< null means "this type has no handlers", which is worth caching too
            };

            /** @brief Holds an entry in place for as long as a dispatch is iterating its table. */
            class pin
            {
            public:
                explicit pin(entry &e) noexcept : m_entry(&e) { ++m_entry->pins; }
                ~pin() { --m_entry->pins; }
                pin(const pin &) = delete;
                pin &operator=(const pin &) = delete;

            private:
                entry *m_entry;
            };

            /** @brief The cached table for this key at this version, or nullptr if it is not cached. */
            [[nodiscard]] entry *find(std::uint64_t state_id, event_type_id type_id, std::uint64_t version) noexcept
            {
                const std::size_t home = index_of(state_id, type_id);
                for (std::size_t probe = 0u; probe < k_probe; ++probe)
                {
                    entry &e = m_entries[(home + probe) & (k_slots - 1u)];
                    if (e.state_id == state_id && e.type_id == type_id && e.version == version)
                        return &e;
                }
                return nullptr;
            }

            /** @brief A slot this key may be installed in, or nullptr when every candidate is pinned. */
            [[nodiscard]] entry *slot_for(std::uint64_t state_id, event_type_id type_id) noexcept
            {
                const std::size_t home = index_of(state_id, type_id);
                for (std::size_t probe = 0u; probe < k_probe; ++probe)
                {
                    entry &e = m_entries[(home + probe) & (k_slots - 1u)];
                    if (e.pins == 0u)
                        return &e;
                }
                return nullptr;
            }

            /**
             * @brief Drops every entry belonging to a dispatcher.
             * @details Called when a dispatcher_state is destroyed, so that destroying a dispatcher releases its handlers
             * (and whatever they captured) then and there on the thread doing the destroying, rather than whenever the
             * cache slot happens to be reused. Other threads' caches can only be reached by their own eviction, which is
             * why this is a promptness measure and not a correctness one - a cached table is immutable and safe to hold.
             */
            void forget(std::uint64_t state_id) noexcept
            {
                for (entry &e : m_entries)
                {
                    if (e.state_id == state_id && e.pins == 0u)
                        e = entry{};
                }
            }

        private:
            static constexpr std::size_t k_slots = 32u; ///< power of two
            static constexpr std::size_t k_probe = 4u;

            [[nodiscard]] static std::size_t index_of(std::uint64_t state_id, event_type_id type_id) noexcept
            {
                std::uint64_t h = state_id * 0x9E3779B97F4A7C15ull;
                h ^= static_cast<std::uint64_t>(type_id) + 0x165667B19E3779F9ull + (h << 6) + (h >> 2);
                return static_cast<std::size_t>(h) & (k_slots - 1u);
            }

            std::array<entry, k_slots> m_entries{};
        };

        /** @brief The calling thread's handler cache. */
        [[nodiscard]] inline handler_cache &thread_handler_cache() noexcept
        {
            static thread_local handler_cache cache;
            return cache;
        }

        /**
         * @class dispatcher_state
         * @brief The shared state behind a dispatcher. Subscriptions hold a weak_ptr to it so they outlive the dispatcher
         * safely.
         *
         * @par Concurrency model
         * The per-type handler tables are immutable and replaced wholesale (copy-on-write). A subscribe or unsubscribe takes
         * the writer lock, builds a new table for the one event type it affects, swaps it in, and bumps a version counter. A
         * publish resolves the table for its event type and then runs the handlers with no lock held at all - and because
         * the table it resolved cannot change, the resolution is cached per thread and revalidated against that version
         * counter, so the steady state costs one load and two comparisons rather than a lock (see handler_cache).
         *
         * Four properties fall out of that, and they are the reason for the shape:
         * - Publishes from different threads run concurrently and, in the steady state, share nothing that is written to.
         *   Adding threads adds throughput instead of contention.
         * - Reentrancy is free rather than something to be defended against. A handler that subscribes, unsubscribes or
         *   publishes is just another caller; because the dispatching thread holds no lock while handlers run, there is no
         *   deadlock to avoid and no deferred-work queue to flush afterwards. A handler added during a dispatch lands in a
         *   new table and so first runs on the next event, which is the same rule as before.
         * - An unsubscribe takes effect on a dispatch already iterating an older table, because the entry is skipped by way
         *   of the shared node's active flag rather than by being removed from the table being walked. On the dispatching
         *   thread that makes it immediate; across threads it is prompt but not a barrier (see subscription).
         * - A publish never blocks behind a running handler, however long that handler takes, on any thread.
         *
         * The cost is paid where it belongs: subscribing is O(handlers of that event type) refcount copies plus a version
         * bump that makes every thread revalidate its cache once. Subscriptions happen at setup; publishes happen in the
         * frame loop.
         */
        class dispatcher_state
        {
        public:
            dispatcher_state() noexcept : m_id(s_next_state_id.fetch_add(1u, std::memory_order_relaxed)) {}
            dispatcher_state(const dispatcher_state &) = delete;
            dispatcher_state &operator=(const dispatcher_state &) = delete;

            ~dispatcher_state() { thread_handler_cache().forget(m_id); }

            [[nodiscard]] std::size_t next_token() noexcept
            {
                return m_next_token.fetch_add(1u, std::memory_order_relaxed);
            }

            void add(event_type_id id, handler_entry &&entry)
            {
                {
                    const std::unique_lock lock(m_mutex);

                    handler_list_ptr &slot = m_handlers[id];
                    auto next = std::make_shared<handler_list>();
                    next->reserve((slot ? slot->size() : 0u) + 1u);

                    // Rebuilding is also when entries whose subscription is gone are dropped, so a dispatcher that is
                    // subscribed to and unsubscribed from repeatedly does not accumulate dead entries.
                    if (slot)
                        copy_live(*slot, *next);

                    insert_sorted(*next, std::move(entry));
                    slot = std::move(next);

                    // Inside the lock: a thread that sees this version is guaranteed to see the table it names.
                    m_version.fetch_add(1u, std::memory_order_release);
                }

                m_bloom.fetch_or(bloom_bit(id), std::memory_order_release);
            }

            void unsubscribe(event_type_id id, std::size_t token) noexcept
            {
                const std::unique_lock lock(m_mutex);

                const auto found = m_handlers.find(id);
                if (found == m_handlers.end())
                    return;

                const handler_list &current = *found->second;
                const auto it = std::find_if(current.begin(), current.end(),
                                             [token](const handler_entry &h) { return h.token == token; });
                if (it == current.end())
                    return;

                // Deactivate before rebuilding. A dispatch on another thread may already be iterating this table; the flag
                // is what stops the handler running for the event it is part-way through.
                it->node->active.store(false, std::memory_order_release);

                try
                {
                    auto next = std::make_shared<handler_list>();
                    next->reserve(current.size());
                    copy_live(current, *next);

                    if (next->empty())
                        m_handlers.erase(found);
                    else
                        found->second = std::move(next);

                    m_version.fetch_add(1u, std::memory_order_release);
                }
                catch (...)
                {
                    // Out of memory while copying the table. The handler is already deactivated and will never run again,
                    // so leaving its entry in place costs one skipped element per publish of this type and nothing else;
                    // the next structural change that does succeed removes it.
                }
            }

            bool publish(const event_base &e)
            {
                const event_type_id id = e.type_id();

                // Cheap rejection for event types nothing has ever subscribed to, which in a large system is most of them.
                // Bits are only ever set, so a miss is conclusive and a hit only means "look properly".
                if ((m_bloom.load(std::memory_order_acquire) & bloom_bit(id)) == 0u)
                    return false;

                const std::uint64_t version = m_version.load(std::memory_order_acquire);
                handler_cache &cache = thread_handler_cache();

                if (handler_cache::entry *cached = cache.find(m_id, id, version))
                {
                    if (!cached->list)
                        return false;

                    // Pinned for the duration: a handler may publish, and a nested publish that misses could otherwise
                    // reuse this slot and free the table being walked here.
                    const handler_cache::pin held(*cached);
                    return invoke_all(*cached->list, e);
                }

                handler_list_ptr handlers;
                {
                    const std::shared_lock lock(m_mutex);

                    if (const auto found = m_handlers.find(id); found != m_handlers.end())
                        handlers = found->second;
                }

                if (handler_cache::entry *slot = cache.slot_for(m_id, id))
                {
                    slot->state_id = m_id;
                    slot->type_id = id;
                    slot->version = version;
                    slot->list = handlers;
                }

                if (!handlers)
                    return false;

                // Iterated through this frame's own reference rather than the cache slot's, so no pin is needed here even
                // when the slot was not available.
                return invoke_all(*handlers, e);
            }

            [[nodiscard]] std::size_t handler_count(event_type_id id) const noexcept
            {
                const std::shared_lock lock(m_mutex);

                const auto found = m_handlers.find(id);
                if (found == m_handlers.end())
                    return 0u;

                return static_cast<std::size_t>(std::count_if(found->second->begin(), found->second->end(),
                                                              [](const handler_entry &h)
                                                              { return h.node->active.load(std::memory_order_acquire); }));
            }

        private:
            /**
             * @brief Runs an immutable handler table against an event, honouring priority order and consumption.
             * @details No lock is held here: handlers may subscribe, unsubscribe, publish, or block, on this thread or any
             * other, without any of it disturbing this iteration, because the table cannot change once published.
             */
            static bool invoke_all(const handler_list &handlers, const event_base &e)
            {
                for (const handler_entry &h : handlers)
                {
                    if (!h.node->active.load(std::memory_order_acquire))
                        continue;

                    if (h.node->invoke(e))
                        return true;
                }

                return false;
            }

            /** @brief The bloom bit an event type id occupies. Collisions only cost a lookup that would have happened. */
            [[nodiscard]] static constexpr std::uint64_t bloom_bit(event_type_id id) noexcept
            {
                return std::uint64_t{1} << (static_cast<std::uint64_t>(id) & 63u);
            }

            static void insert_sorted(handler_list &entries, handler_entry &&entry)
            {
                // Descending priority; a new entry goes after existing entries of equal priority (stable).
                const auto pos = std::upper_bound(entries.begin(), entries.end(), entry.priority,
                                                  [](int p, const handler_entry &h) { return p > h.priority; });
                entries.insert(pos, std::move(entry));
            }

            /** @brief Appends the still-subscribed entries of @p from to @p to, preserving order. */
            static void copy_live(const handler_list &from, handler_list &to)
            {
                for (const handler_entry &h : from)
                    if (h.node->active.load(std::memory_order_acquire))
                        to.push_back(h);
            }

            mutable std::shared_mutex m_mutex;
            std::unordered_map<event_type_id, handler_list_ptr> m_handlers;

            /**
             * @var m_bloom
             * @brief One bit per event type id modulo 64, set when the type first gains a handler and never cleared. Lets
             * publish reject an event type nobody listens for without touching the lock or the hash map.
             */
            std::atomic<std::uint64_t> m_bloom{0u};

            /**
             * @var m_version
             * @brief Bumped by every structural change, under the writer lock. A thread's cached handler tables are valid
             * exactly while this still reads as it did when they were resolved.
             */
            std::atomic<std::uint64_t> m_version{1u};

            /**
             * @var m_id
             * @brief Process-unique identity, so a thread's cache cannot confuse this dispatcher with a later one that
             * happens to be allocated at the same address.
             */
            std::uint64_t m_id;

            std::atomic<std::size_t> m_next_token{1u};

            inline static std::atomic<std::uint64_t> s_next_state_id{1u};
        };
    } // namespace detail

    /**
     * @class dispatcher
     * @brief Synchronous publish/subscribe hub, usable from any number of threads.
     *
     * - Handlers for an event run in descending priority order (ties: subscription order). A handler that returns `true`
     *   consumes the event and stops propagation; `void` handlers never consume.
     * - Fully reentrant: handlers may subscribe, unsubscribe (including themselves) and publish. Handlers added during a
     *   dispatch first run on the *next* event; a handler unsubscribed during a dispatch does not run for the event in
     *   flight, even if it comes later in the same priority order. (That is the guarantee on the dispatching thread; see
     *   subscription for what unsubscribing from another thread does and does not stop.)
     * - Publishing costs O(handlers of that event type); an event type with no handlers is rejected without taking a lock.
     * - Exceptions thrown by handlers propagate to the publisher; the dispatcher remains consistent.
     * - Subscriptions may outlive the dispatcher.
     *
     * @par Threading
     * Every member is safe to call from any thread, concurrently, with no external synchronisation, and so is
     * subscription::reset. Publishes do not block one another except for a hash lookup, and never block behind a running
     * handler. What the dispatcher does *not* do is serialise the handlers themselves: publishing the same event type from
     * two threads runs its handlers on both threads at once, so a handler that touches shared state must synchronise it,
     * exactly as any other function called from two threads would. An application that would rather keep its handlers
     * single-threaded should have its threads push to an event_queue and drain that queue on one thread.
     *
     * A moved-from dispatcher may only be destroyed or assigned to.
     */
    class dispatcher
    {
    public:
        dispatcher() : m_state(std::make_shared<detail::dispatcher_state>()) {}

        dispatcher(const dispatcher &) = delete;
        dispatcher &operator=(const dispatcher &) = delete;
        dispatcher(dispatcher &&) noexcept = default;
        dispatcher &operator=(dispatcher &&) noexcept = default;
        ~dispatcher() = default;

        /**
         * @brief Subscribes a plain function that does not consume events.
         * @param priority Higher values run first. Default 0.
         */
        template <typename T>
        [[nodiscard]] subscription subscribe(void (*callback)(const T &), int priority = 0)
        {
            static_assert(registered_event<T>, "T must derive from catalyst::core::event<T> (or otherwise provide a static type_id())");
            return add_handler<T>(callback, priority);
        }

        /**
         * @brief Subscribes a plain function that may consume events by returning true.
         * @param priority Higher values run first. Default 0.
         */
        template <typename T>
        [[nodiscard]] subscription subscribe(bool (*callback)(const T &), int priority = 0)
        {
            static_assert(registered_event<T>, "T must derive from catalyst::core::event<T> (or otherwise provide a static type_id())");
            return add_handler<T>(callback, priority);
        }

        /**
         * @brief Subscribes any callable taking `const T&`. Returning void never consumes; returning something convertible
         * to bool consumes when true.
         * @param priority Higher values run first. Default 0.
         */
        template <typename T, typename Callback>
            requires(registered_event<T> &&
                     event_callback_for<Callback, T> &&
                     std::copy_constructible<std::decay_t<Callback>> &&
                     (!function_pointer<Callback>))
        [[nodiscard]] subscription subscribe(Callback &&callback, int priority = 0)
        {
            return add_handler<T>(std::forward<Callback>(callback), priority);
        }

        /**
         * @brief Delivers an event to its handlers synchronously, on the calling thread.
         * @return true if a handler consumed the event.
         */
        bool publish(const event_base &e) { return m_state->publish(e); }

        /** @brief Typed convenience overload of publish. */
        template <typename T>
        bool publish(const T &e)
        {
            static_assert(event_type<T>, "T must derive from catalyst::core::event_base");
            return m_state->publish(static_cast<const event_base &>(e));
        }

        /** @brief Removes a handler by type id and token. Normally done through subscription; kept for manual management. */
        void unsubscribe(event_type_id id, std::size_t token) noexcept { m_state->unsubscribe(id, token); }

        /** @brief Number of live handlers registered for an event type. */
        [[nodiscard]] std::size_t handler_count(event_type_id id) const noexcept { return m_state->handler_count(id); }

        template <typename T>
            requires(registered_event<T>)
        [[nodiscard]] std::size_t handler_count() const noexcept
        {
            return m_state->handler_count(T::type_id());
        }

        /**
         * @brief No-op, retained so existing calls keep compiling.
         * @deprecated The dispatcher no longer has an owner thread to hand over: every operation is safe from any thread.
         */
        void adopt_current_thread() noexcept {}

    private:
        template <typename T, typename Callback>
        subscription add_handler(Callback &&callback, int priority)
        {
            using callback_t = std::decay_t<Callback>;
            using result_t = std::invoke_result_t<callback_t &, const T &>;

            const event_type_id id = T::type_id();

            auto node = std::make_shared<detail::handler_node>();
            node->invoke = [cb = callback_t(std::forward<Callback>(callback))](const event_base &base) mutable -> bool
            {
                const T &e = static_cast<const T &>(base);
                if constexpr (std::is_void_v<result_t>)
                {
                    std::invoke(cb, e);
                    return false;
                }
                else
                {
                    return static_cast<bool>(std::invoke(cb, e));
                }
            };

            detail::handler_entry entry;
            entry.token = m_state->next_token();
            entry.priority = priority;
            entry.node = node;

            const std::size_t token = entry.token;
            m_state->add(id, std::move(entry));
            return subscription(m_state, id, token, std::move(node));
        }

        std::shared_ptr<detail::dispatcher_state> m_state;
    };

} // namespace catalyst::core
