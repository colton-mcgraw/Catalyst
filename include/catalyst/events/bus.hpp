#pragma once

#include <catalyst/events/concepts.hpp>
#include <catalyst/events/detail/chain.hpp>
#include <catalyst/events/detail/registry.hpp>
#include <catalyst/events/next.hpp>
#include <catalyst/events/tag.hpp>
#include <catalyst/events/task.hpp>
#include <catalyst/events/token.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <utility>

namespace catalyst::events
{

    // ------------------------------------------------------------
    // Event Bus
    // ------------------------------------------------------------
    //
    // Ordering: middleware and listeners run in descending priority; equal
    // priorities run in registration order. The highest-priority middleware is
    // the outermost layer of the chain.
    //
    // Threading: every member function, and every token operation, may be
    // called concurrently from any thread. No lock is held while a listener or
    // middleware runs, so callables are free to register or remove anything.
    // A dispatch works on a snapshot taken when it starts: registrations added
    // during it are first seen by the next dispatch, and a removal stops the
    // callable from being started again, even later in the same dispatch. An
    // invocation already running on another thread when remove() returns may
    // still be in progress; the callable object itself stays alive until every
    // in-flight dispatch that captured it has finished. Concurrent dispatches
    // may invoke the same callable concurrently, so callables that share state
    // must synchronise it themselves.
    //
    // Dispatch takes no lock and performs no reference counting: it is two
    // atomic increments on a per-thread counter plus the lookups, so it scales
    // with the number of dispatching threads. The cost moves to the writers:
    // add and remove take a mutex, and a registration removed while any dispatch
    // on this bus is in flight is freed by a later add or remove, or by the bus's
    // destructor, rather than the instant that dispatch ends. See
    // detail/registry.hpp for the mechanism.
    //
    // Lifetime: tokens may outlive the bus; they simply become invalid. The bus
    // must outlive its own in-flight dispatches.

    /**
     * @class bus
     * @brief Event bus for managing listeners and middleware.
     */
    class bus
    {
    public:
        bus() = default;
        bus(const bus &) = delete;
        bus &operator=(const bus &) = delete;

        // --------------------------------------------------------
        // Listeners
        // --------------------------------------------------------

        template <typename Event, typename F>
            requires listener_for<F, Event>
        token add_listener(F &&f, int priority = 0)
        {
            return install<detail::sync_listener_sig>(detail::kind::sync_listener, detail::key_of<Event>(), priority,
                                                      [fn = std::forward<F>(f)](const void *ptr) mutable
                                                      { fn(*static_cast<const Event *>(ptr)); });
        }

        template <typename Event, typename F>
            requires coroutine_listener_for<F, Event>
        token add_listener(F &&f, int priority = 0)
        {
            return install<detail::async_listener_sig>(detail::kind::async_listener, detail::key_of<Event>(), priority,
                                                       [fn = std::forward<F>(f)](const void *ptr) mutable -> task<void>
                                                       { return fn(*static_cast<const Event *>(ptr)); });
        }

        // --------------------------------------------------------
        // Middleware
        // --------------------------------------------------------

        template <typename Event, typename F>
            requires chain_middleware_for<F, Event>
        token add_middleware(F &&f, int priority = 0)
        {
            return install<detail::sync_middleware_sig>(
                detail::kind::sync_middleware, detail::key_of<Event>(), priority,
                [fn = std::forward<F>(f)](void *ev, void *ctx, void (*resume)(void *, void *)) mutable
                { fn(*static_cast<Event *>(ev), next<Event>{detail::chain_access{}, ctx, resume}); });
        }

        template <typename Event, typename F>
            requires middleware_for<F, Event>
        token add_middleware(F &&f, int priority = 0)
        {
            return add_middleware<Event>(
                [fn = std::forward<F>(f)](Event &e, const next<Event> &n) mutable
                {
                    if (fn(e))
                        n(e);
                },
                priority);
        }

        template <typename Event, typename F>
            requires async_chain_middleware_for<F, Event>
        token add_async_middleware(F &&f, int priority = 0)
        {
            return install<detail::async_middleware_sig>(
                detail::kind::async_middleware, detail::key_of<Event>(), priority,
                // A coroutine rather than a plain forwarder so that the
                // async_next outlives the user's coroutine, which only
                // holds a reference to it.
                [fn = std::forward<F>(f)](void *ev, void *ctx,
                                          task<void> (*resume)(void *, void *)) mutable -> task<void>
                {
                    async_next<Event> n{detail::chain_access{}, ctx, resume};
                    co_await fn(*static_cast<Event *>(ev), n);
                });
        }

        template <typename Event, typename F>
            requires async_middleware_for<F, Event>
        token add_async_middleware(F &&f, int priority = 0)
        {
            return add_async_middleware<Event>(
                [fn = std::forward<F>(f)](Event &e, const async_next<Event> &n) mutable -> task<void>
                {
                    if (co_await fn(e))
                        co_await n(e);
                },
                priority);
        }

        // --------------------------------------------------------
        // Dispatch
        // --------------------------------------------------------

        template <typename Event>
        void dispatch(Event e)
        {
            detail::read_guard guard{*state_};

            const detail::entry *en = state_->find(detail::key_of<Event>());
            if (!en)
                return;

            detail::chain_ctx ctx{en->sync_middleware.load(std::memory_order_seq_cst),
                                  en->sync_listeners.load(std::memory_order_seq_cst), 0};
            if (!ctx.middleware && !ctx.listeners)
                return;

            detail::run_chain(&ctx, &e);
        }

        template <typename Event>
        task<void> dispatch_async(Event e)
        {
            detail::pinned<detail::async_middleware_sig> middleware;
            detail::pinned<detail::async_listener_sig> listeners;
            {
                // The guard cannot be held across a suspension, so pin the lists
                // with their own reference counts and let the guard go.
                detail::read_guard guard{*state_};
                if (const detail::entry *en = state_->find(detail::key_of<Event>()))
                {
                    middleware = detail::pinned<detail::async_middleware_sig>{
                        en->async_middleware.load(std::memory_order_seq_cst)};
                    listeners =
                        detail::pinned<detail::async_listener_sig>{en->async_listeners.load(std::memory_order_seq_cst)};
                }
            }

            if (!middleware.get() && !listeners.get())
                co_return;

            detail::async_chain_ctx ctx{middleware.get(), listeners.get(), 0};
            co_await detail::run_chain_async(&ctx, &e);
        }

    private:
        template <typename Signature, typename Callable>
        token install(detail::kind k, const detail::hashed_key &type, int priority, Callable &&callable)
        {
            auto s = detail::make_slot<Signature>(state_->next_id.fetch_add(1, std::memory_order_relaxed), priority,
                                                  std::forward<Callable>(callable));
            {
                std::lock_guard lock(state_->mutex);
                state_->add<Signature>(type, s);
            }
            return token{state_, s, k, type};
        }

        std::shared_ptr<detail::state> state_ = std::make_shared<detail::state>();
    };

} // namespace catalyst::events
