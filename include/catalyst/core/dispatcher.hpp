/**
 * @file dispatcher.hpp
 * @brief Defines the dispatcher class, which manages event subscriptions and delivers published events to the matching
 * handlers, in priority order, with support for event consumption and full reentrancy (handlers may subscribe, unsubscribe
 * and publish while a dispatch is in progress).
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include "event.hpp"
#include "subscription.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * @def CATALYST_DISPATCHER_THREAD_CHECKS
 * @brief When non-zero (the default in debug builds), every dispatcher operation asserts that it runs on the thread that
 * created the dispatcher. Define CATALYST_DISPATCHER_NO_THREAD_CHECKS to opt out.
 */
#if !defined(NDEBUG) && !defined(CATALYST_DISPATCHER_NO_THREAD_CHECKS)
#define CATALYST_DISPATCHER_THREAD_CHECKS 1
#else
#define CATALYST_DISPATCHER_THREAD_CHECKS 0
#endif

namespace catalyst::core
{
    namespace detail
    {
        /** @brief One registered handler. */
        struct handler_entry
        {
            std::size_t token = 0u;
            int priority = 0;
            std::shared_ptr<std::atomic_bool> active;
            std::function<bool(const event_base &)> invoke;
        };

        /** @brief All handlers for one event type, sorted by descending priority (stable for equal priorities). */
        struct handler_list
        {
            std::vector<handler_entry> entries;
            std::size_t dead = 0u; ///< Entries deactivated while a dispatch was in progress; compacted at depth 0.
        };

        /**
         * @class dispatcher_state
         * @brief The shared state behind a dispatcher. Subscriptions hold a weak_ptr to it so they outlive the dispatcher safely.
         *
         * Reentrancy model: while a dispatch is in progress (m_depth > 0) the handler tables are never structurally modified.
         * New subscriptions go to m_pending and unsubscribes only flip the active flag and record the list as dirty. When the
         * outermost dispatch returns, flush() compacts exactly the dirty lists and merges the pending entries - so the cost of
         * a publish is proportional to the subscribers of *that* event type, never to the size of the whole dispatcher.
         */
        class dispatcher_state
        {
        public:
            dispatcher_state() = default;
            dispatcher_state(const dispatcher_state &) = delete;
            dispatcher_state &operator=(const dispatcher_state &) = delete;

            [[nodiscard]] std::size_t next_token() noexcept { return m_next_token++; }

            void add(event_type_id id, handler_entry &&entry)
            {
                assert_owner_thread();

                if (m_depth > 0u)
                {
                    // Deferred: the vector being iterated must not reallocate. The new handler will not see the event
                    // currently being dispatched.
                    m_pending.emplace_back(id, std::move(entry));
                    return;
                }

                maybe_flush();
                insert_sorted(m_handlers[id].entries, std::move(entry));
            }

            void unsubscribe(event_type_id id, std::size_t token) noexcept
            {
                assert_owner_thread();

                for (auto it = m_pending.begin(); it != m_pending.end(); ++it)
                {
                    if (it->first == id && it->second.token == token)
                    {
                        m_pending.erase(it);
                        return;
                    }
                }

                const auto found = m_handlers.find(id);
                if (found == m_handlers.end())
                    return;

                handler_list &list = found->second;
                const auto it = std::find_if(list.entries.begin(), list.entries.end(),
                                             [token](const handler_entry &h) { return h.token == token; });
                if (it == list.entries.end())
                    return;

                if (it->active)
                    it->active->store(false, std::memory_order_relaxed);

                if (m_depth > 0u)
                {
                    mark_dirty(id, list);
                    return;
                }

                list.entries.erase(it);
                if (list.entries.empty())
                    m_handlers.erase(found);
            }

            bool publish(const event_base &e)
            {
                assert_owner_thread();

                if (m_depth == 0u)
                    maybe_flush(); // Picks up anything left behind if a previous dispatch exited via an exception.

                const auto found = m_handlers.find(e.type_id());
                if (found == m_handlers.end())
                    return false;

                handler_list &list = found->second;
                bool consumed = false;
                {
                    const depth_guard guard(m_depth);

                    // The list cannot grow or shrink while m_depth > 0, so indexing over a fixed count is safe even if a
                    // handler subscribes/unsubscribes/publishes recursively.
                    const std::size_t count = list.entries.size();
                    for (std::size_t i = 0u; i < count; ++i)
                    {
                        const handler_entry &h = list.entries[i];
                        if (!h.active || !h.active->load(std::memory_order_relaxed))
                            continue;

                        if (h.invoke(e))
                        {
                            consumed = true;
                            break;
                        }
                    }
                }

                if (m_depth == 0u)
                    maybe_flush();

                return consumed;
            }

            [[nodiscard]] std::size_t handler_count(event_type_id id) const noexcept
            {
                std::size_t n = 0u;
                if (const auto found = m_handlers.find(id); found != m_handlers.end())
                {
                    n += static_cast<std::size_t>(std::count_if(found->second.entries.begin(), found->second.entries.end(),
                                                                [](const handler_entry &h)
                                                                { return h.active && h.active->load(std::memory_order_relaxed); }));
                }
                for (const auto &p : m_pending)
                    if (p.first == id)
                        ++n;
                return n;
            }

            void adopt_current_thread() noexcept
            {
#if CATALYST_DISPATCHER_THREAD_CHECKS
                m_owner = std::this_thread::get_id();
#endif
            }

        private:
            using pending_entry = std::pair<event_type_id, handler_entry>;

            /** @brief Keeps m_depth balanced even when a handler throws. */
            struct depth_guard
            {
                std::size_t &depth;
                explicit depth_guard(std::size_t &d) noexcept : depth(d) { ++depth; }
                ~depth_guard() { --depth; }
                depth_guard(const depth_guard &) = delete;
                depth_guard &operator=(const depth_guard &) = delete;
            };

            static void insert_sorted(std::vector<handler_entry> &entries, handler_entry &&entry)
            {
                // Descending priority; a new entry goes after existing entries of equal priority (stable).
                const auto pos = std::upper_bound(entries.begin(), entries.end(), entry.priority,
                                                  [](int p, const handler_entry &h) { return p > h.priority; });
                entries.insert(pos, std::move(entry));
            }

            static void compact(handler_list &list) noexcept
            {
                list.entries.erase(std::remove_if(list.entries.begin(), list.entries.end(),
                                                  [](const handler_entry &h)
                                                  { return !h.active || !h.active->load(std::memory_order_relaxed); }),
                                   list.entries.end());
                list.dead = 0u;
            }

            void mark_dirty(event_type_id id, handler_list &list) noexcept
            {
                if (list.dead++ != 0u)
                    return;

                try
                {
                    m_dirty.push_back(id);
                }
                catch (...)
                {
                    m_dirty_overflow = true; // Out of memory: fall back to a full scan on the next flush.
                }
            }

            [[nodiscard]] bool needs_flush() const noexcept
            {
                return !m_pending.empty() || !m_dirty.empty() || m_dirty_overflow;
            }

            /** @brief Inline fast-path check so the common "nothing changed" publish never calls the out-of-line flush. */
            void maybe_flush()
            {
                if (needs_flush())
                    flush();
            }

            /** @brief Compacts dirty lists and merges pending subscriptions. Only ever called at depth 0. */
            void flush()
            {
                if (m_dirty_overflow)
                {
                    for (auto it = m_handlers.begin(); it != m_handlers.end();)
                    {
                        compact(it->second);
                        it = it->second.entries.empty() ? m_handlers.erase(it) : std::next(it);
                    }
                    m_dirty.clear();
                    m_dirty_overflow = false;
                }
                else if (!m_dirty.empty())
                {
                    for (const event_type_id id : m_dirty)
                    {
                        const auto found = m_handlers.find(id);
                        if (found == m_handlers.end())
                            continue;

                        compact(found->second);
                        if (found->second.entries.empty())
                            m_handlers.erase(found);
                    }
                    m_dirty.clear();
                }

                if (!m_pending.empty())
                {
                    // Merge in subscription order. If an insert throws, the entries already merged are removed from
                    // m_pending and the rest stay queued for the next flush - nothing is lost or duplicated.
                    std::size_t done = 0u;
                    struct erase_done
                    {
                        std::vector<pending_entry> &v;
                        std::size_t &n;
                        ~erase_done() { v.erase(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(n)); }
                    } guard{m_pending, done};

                    while (done < m_pending.size())
                    {
                        pending_entry &p = m_pending[done];
                        insert_sorted(m_handlers[p.first].entries, std::move(p.second));
                        ++done;
                    }
                }
            }

            void assert_owner_thread() const noexcept
            {
#if CATALYST_DISPATCHER_THREAD_CHECKS
                assert(std::this_thread::get_id() == m_owner &&
                       "catalyst::core::dispatcher used from a thread other than its owner; post events through "
                       "concurrent_event_queue instead, or call adopt_current_thread() if ownership really moved");
#endif
            }

            std::unordered_map<event_type_id, handler_list> m_handlers;
            std::vector<pending_entry> m_pending;
            std::vector<event_type_id> m_dirty;
            bool m_dirty_overflow = false;
            std::size_t m_next_token = 1u;
            std::size_t m_depth = 0u;
#if CATALYST_DISPATCHER_THREAD_CHECKS
            std::thread::id m_owner = std::this_thread::get_id();
#endif
        };
    } // namespace detail

    /**
     * @class dispatcher
     * @brief Synchronous, single-threaded publish/subscribe hub.
     *
     * - Handlers for an event run in descending priority order (ties: subscription order). A handler that returns `true`
     *   consumes the event and stops propagation; `void` handlers never consume.
     * - Fully reentrant: handlers may subscribe, unsubscribe (including themselves) and publish. Handlers added during a
     *   dispatch first run on the *next* event.
     * - Publishing costs O(handlers of that event type); unsubscribing costs O(handlers of that type).
     * - Exceptions thrown by handlers propagate to the publisher; the dispatcher remains consistent.
     * - Subscriptions may outlive the dispatcher.
     *
     * Threading: all member functions (and subscription::reset) must be called on the owner thread - by default the thread
     * that constructed the dispatcher (debug builds assert this). Producers on other threads should post events through
     * a concurrent_event_queue that the owner thread drains.
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
         * @brief Delivers an event to its handlers synchronously.
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

        /** @brief Number of live handlers registered for an event type (including ones added during an in-flight dispatch). */
        [[nodiscard]] std::size_t handler_count(event_type_id id) const noexcept { return m_state->handler_count(id); }

        template <typename T>
            requires(registered_event<T>)
        [[nodiscard]] std::size_t handler_count() const noexcept
        {
            return m_state->handler_count(T::type_id());
        }

        /** @brief Makes the calling thread the owner for the debug thread checks (no-op in release). */
        void adopt_current_thread() noexcept { m_state->adopt_current_thread(); }

    private:
        template <typename T, typename Callback>
        subscription add_handler(Callback &&callback, int priority)
        {
            using callback_t = std::decay_t<Callback>;
            using result_t = std::invoke_result_t<callback_t &, const T &>;

            const event_type_id id = T::type_id();
            const std::size_t token = m_state->next_token();
            auto active = std::make_shared<std::atomic_bool>(true);

            detail::handler_entry entry;
            entry.token = token;
            entry.priority = priority;
            entry.active = active;
            entry.invoke = [cb = callback_t(std::forward<Callback>(callback))](const event_base &base) mutable -> bool
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

            m_state->add(id, std::move(entry));
            return subscription(m_state, id, token, std::move(active));
        }

        std::shared_ptr<detail::dispatcher_state> m_state;
    };

} // namespace catalyst::core
