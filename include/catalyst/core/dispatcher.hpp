/**
 * @file dispatcher.hpp
 * @brief Defines the dispatcher class, which is responsible for managing event subscriptions and dispatching events to the appropriate handlers. The dispatcher allows clients to subscribe to specific event types with callback functions, and it ensures that when an event is published, all relevant handlers are invoked. The dispatcher also handles the lifecycle of subscriptions, allowing for safe unsubscription and cleanup of inactive handlers.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include "event.hpp"
#include "subscription.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * @namespace catalyst::core
 * @brief The catalyst::core namespace contains core functionalities of the Catalyst framework, including event handling and dispatching.
 */
namespace catalyst::core
{
    /**
     * @class dispatcher
     * @brief Manages event subscriptions and dispatches events to the appropriate handlers. Clients can subscribe to specific event types with callback functions, and the dispatcher ensures that when an event is published, all relevant handlers are invoked. The dispatcher also handles the lifecycle of subscriptions, allowing for safe unsubscription and cleanup of inactive handlers.
     */
    class dispatcher
    {
    public:
        /**
         * @fn dispatcher
         * @brief Default constructor initializes an empty dispatcher with no subscriptions. The dispatcher is ready to accept subscriptions and publish events after construction.
         */
        dispatcher() = default;
        /**
         * @fn dispatcher(const dispatcher &) = delete
         * @brief Copy constructor is deleted to prevent copying of dispatcher objects. Dispatchers manage resources related to event subscriptions, and copying them could lead to issues with resource management and unintended behavior. If you need to share a dispatcher, consider using a pointer or reference instead of copying.
         */
        dispatcher(const dispatcher &) = delete;
        /**
         * @fn operator=(const dispatcher &) = delete
         * @brief Copy assignment operator is deleted for the same reasons as the copy constructor. Dispatchers should not be copyable to ensure proper management of event subscription resources and to prevent unintended behavior.
         */
        dispatcher &operator=(const dispatcher &) = delete;

        /**
         * @fn subscribe
         * @brief Subscribes a callback function to a specific event type. The callback will be invoked whenever an event of the specified type is published. The subscription is represented by a subscription object, which can be used to manage the lifecycle of the subscription, including unsubscribing from the event when the subscription is destroyed or reset.
         * @tparam T The type of event to subscribe to. This must derive from catalyst::core::event_base.
         * @param callback A pointer to a function that takes a const reference to an event of type T and returns void. This function will be called when an event of the specified type is published.
         * @return A subscription object that represents the active subscription. You can use this object to unsubscribe from the event later or let it go out of scope to automatically unsubscribe.
         */
        template <typename T>
        [[nodiscard]] subscription subscribe(void (*callback)(const T &))
        {
            static_assert(registered_event<T>, "T must derive from catalyst::core::event<> (or otherwise provide a static type_id())");

            const event_type_id id = T::type_id();
            const std::size_t token = m_next_token++;
            auto active = std::make_shared<std::atomic_bool>(true);

            handler_entry entry;
            entry.token = token;
            entry.active = active;
            entry.invoke = [callback](const event_base &base)
            {
                callback(static_cast<const T &>(base));
            };

            m_handlers[id].push_back(std::move(entry));
            return subscription(this, id, token, std::move(active));
        }

        /**
         * @fn subscribe
         * @brief Subscribes a callback function to a specific event type. The callback will be invoked whenever an event of the specified type is published. The subscription is represented by a subscription object, which can be used to manage the lifecycle of the subscription, including unsubscribing from the event when the subscription is destroyed or reset.
         * @tparam T The type of event to subscribe to. This must derive from catalyst::core::event_base.
         * @tparam Callback The type of the callback function or callable object. This must be compatible with the event type T and must be copy constructible. If Callback is a function pointer, it will be handled by the other subscribe overload.
         * @param callback A callback function or callable object that will be called when an event of the specified type is published. The callback must be compatible with the event type T and must be copy constructible. If Callback is a function pointer, it will be handled by the other subscribe overload.
         * @return A subscription object that represents the active subscription. You can use this object to unsubscribe from the event later or let it go out of scope to automatically unsubscribe.
         */
        template <typename T, typename Callback>
            requires(registered_event<T> &&
                     event_callback_for<Callback, T> &&
                     std::copy_constructible<std::decay_t<Callback>> &&
                     (!function_pointer<Callback>))
        [[nodiscard]] subscription subscribe(Callback &&callback)
        {
            using callback_t = std::decay_t<Callback>;

            const event_type_id id = T::type_id();
            const std::size_t token = m_next_token++;
            auto active = std::make_shared<std::atomic_bool>(true);

            callback_t cb(std::forward<Callback>(callback));

            handler_entry entry;
            entry.token = token;
            entry.active = active;
            entry.invoke = [cb = std::move(cb)](const event_base &base) mutable
            {
                std::invoke(cb, static_cast<const T &>(base));
            };

            m_handlers[id].push_back(std::move(entry));
            return subscription(this, id, token, std::move(active));
        }

        /**
         * @fn publish
         * @brief Publishes an event to all subscribed handlers. When an event is published, the dispatcher looks up all handlers subscribed to the event's type and invokes them with the event as an argument. The dispatcher also manages the dispatch depth to ensure that if handlers publish events while handling another event, it can safely clean up any inactive handlers after the outermost dispatch is complete.
         * @param e A const reference to the event to be published. The event must derive from catalyst::core::event_base and have a valid type ID.
         */
        void publish(const event_base &e)
        {
            const auto found = m_handlers.find(e.type_id());
            if (found == m_handlers.end())
                return;

            ++m_dispatch_depth;
            for (const auto &handler : found->second)
            {
                if (handler.active && handler.active->load(std::memory_order_relaxed))
                    handler.invoke(e);
            }
            --m_dispatch_depth;

            if (m_dispatch_depth == 0u)
                cleanup_dead();
        }

        /**
         * @fn publish
         * @brief Overload of the publish function that allows publishing an event of a specific type without needing to cast it to event_base. This is a convenience function that forwards the call to the main publish function after ensuring that the event type is valid.
         * @tparam T The type of event to publish. This must derive from catalyst::core::event_base.
         * @param e A const reference to the event to be published. The event must derive from catalyst::core::event_base and have a valid type ID.
         */
        template <typename T>
        void publish(const T &e)
        {
            static_assert(event_type<T>, "T must derive from catalyst::core::event_base");
            publish(static_cast<const event_base &>(e));
        }

        /**
         * @fn unsubscribe
         * @brief Unsubscribes a handler from a specific event type using the event type ID and subscription token. This function is called by the subscription object when it is reset or destroyed to ensure that the associated handler is no longer active and will not be invoked when the event is published. The dispatcher also manages the cleanup of inactive handlers after unsubscription.
         * @param id The unique event type ID associated with the subscription to be unsubscribed. This is used by the dispatcher to identify which handlers to remove from the list of active handlers for that event type.
         * @param token The unique token that identifies the specific subscription to be unsubscribed within the dispatcher. This token is used to find and deactivate the corresponding handler entry in the dispatcher's internal data structures.
         */
        void unsubscribe(event_type_id id, std::size_t token) noexcept
        {
            const auto found = m_handlers.find(id);
            if (found == m_handlers.end())
                return;

            for (auto &handler : found->second)
            {
                if (handler.token == token)
                {
                    if (handler.active)
                        handler.active->store(false, std::memory_order_relaxed);
                    break;
                }
            }

            if (m_dispatch_depth == 0u)
                cleanup_dead();
        }

    private:
        /**
         * @struct handler_entry
         * @brief Represents an individual event handler subscription. This structure contains the unique token for the subscription, a shared pointer to an atomic boolean that indicates whether the subscription is currently active, and a std::function that can be invoked to call the subscribed handler with the appropriate event type. The dispatcher uses this structure to manage and invoke event handlers when events are published.
         */
        struct handler_entry
        {
            /**
             * @var token
             * @brief A unique token that identifies this subscription within the dispatcher. This token is used by the dispatcher to identify which handler to deactivate when a subscription is unsubscribed. Each subscription receives a different token, and the dispatcher uses this token in conjunction with the event type ID to manage active handlers.
             */
            std::size_t token = 0u;
            /**
             * @var active
             * @brief A shared pointer to an atomic boolean that indicates whether this subscription is currently active. This allows the dispatcher to manage the active state of the subscription and ensure that event handlers are not called after they have been unsubscribed. When a subscription is unsubscribed, this atomic boolean is set to false, and the dispatcher will skip invoking this handler for future events.
             */
            std::shared_ptr<std::atomic_bool> active;
            /**
             * @var invoke
             * @brief A std::function that can be invoked to call the subscribed handler with the appropriate event type. This function takes a const reference to an event_base and is responsible for casting it to the correct event type and calling the user's callback function. The dispatcher uses this function to invoke the handler when an event of the corresponding type is published.
             */
            std::function<void(const event_base &)> invoke;
        };

        /**
         * @fn cleanup_dead
         * @brief Cleans up inactive handlers from the dispatcher's internal data structures. This function is called after publishing events and after unsubscribing handlers to ensure that any handlers that have been marked as inactive (i.e., their active atomic boolean is set to false) are removed from the list of handlers for their respective event types. This helps to prevent memory leaks and ensures that the dispatcher only maintains active handlers.
         */
        void cleanup_dead()
        {
            for (auto it = m_handlers.begin(); it != m_handlers.end();)
            {
                auto &vec = it->second;
                vec.erase(std::remove_if(vec.begin(), vec.end(), [](const handler_entry &h)
                                         { return !h.active || !h.active->load(std::memory_order_relaxed); }),
                          vec.end());

                if (vec.empty())
                    it = m_handlers.erase(it);
                else
                    ++it;
            }
        }

        /**
         * @var m_handlers
         * @brief An unordered map that associates event type IDs with vectors of handler entries. Each key in the map is a unique event type ID, and the corresponding value is a vector of handler_entry structures that represent the active subscriptions for that event type. The dispatcher uses this map to look up which handlers to invoke when an event is published and to manage subscriptions when they are added or removed.
         */
        std::unordered_map<event_type_id, std::vector<handler_entry>> m_handlers;
        /**
         * @var m_next_token
         * @brief A counter used to generate unique tokens for each subscription. Each time a new subscription is created, this counter is incremented to ensure that each subscription receives a different token. The dispatcher uses these tokens to manage and identify subscriptions when they are unsubscribed.
         */
        std::size_t m_next_token = 1u;
        /**
         * @var m_dispatch_depth
         * @brief A counter that tracks the current depth of event dispatching. This is used to manage the cleanup of inactive handlers safely. If handlers publish events while handling another event, the dispatcher can defer cleanup of inactive handlers until the outermost dispatch is complete, ensuring that it does not modify the list of handlers while it is iterating through them.
         */
        std::size_t m_dispatch_depth = 0u;
    };

}