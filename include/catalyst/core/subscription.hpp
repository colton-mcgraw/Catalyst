/**
 * @file subscription.hpp
 * @brief Defines the subscription class, which represents a handle to an active event subscription in the catalyst::core module. A subscription allows you to manage the lifecycle of an event handler, including unsubscribing from events when the subscription is destroyed or reset. The subscription class is designed to work with the dispatcher class, which manages event handlers and dispatches events to them.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include "event.hpp"

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
 * @brief The catalyst::core namespace contains core functionalities of the Catalyst framework, including event handling
 */
namespace catalyst::core
{
    /**
     * @class subscription
     * @brief Represents a handle to an active event subscription. It allows you to manage the lifecycle of an event handler, including unsubscribing from events when the subscription is destroyed or reset.
     * @details The subscription class is designed to work with the dispatcher class, which manages event handlers and dispatches events to them. When you subscribe to an event using the dispatcher, you receive a subscription object that you can use to unsubscribe from the event later. The subscription class uses RAII principles to ensure that resources are properly released when the subscription goes out of scope.
     */
    class subscription
    {
    public:
        /**
         * @fn subscription
         * @brief Default constructor creates an invalid subscription. You can check if a subscription is valid using the valid() member function.
         */
        subscription() = default;

        /**
         * @fn subscription(const subscription &) = delete
         * @brief Copy constructor is deleted to prevent copying of subscription objects. Subscriptions are meant to be unique handles to event handlers, and copying them could lead to issues with resource management and unintended behavior.
         */
        subscription(const subscription &) = delete;
        /**
         * @fn operator=(const subscription &) = delete
         * @brief Copy assignment operator is deleted for the same reasons as the copy constructor. Subscriptions should not be copyable to ensure proper management of event handler lifecycles.
         */
        subscription &operator=(const subscription &) = delete;

        /**
         * @fn subscription(subscription &&other) noexcept
         * @brief Move constructor transfers ownership of the subscription from another subscription object. After the move, the other subscription becomes invalid.
         * @param other The subscription object to move from.
         */
        subscription(subscription &&other) noexcept;
        /**
         * @fn operator=(subscription &&other) noexcept
         * @brief Move assignment operator transfers ownership of the subscription from another subscription object. After the move, the other subscription becomes invalid.
         * @param other The subscription object to move from.
         * @return A reference to this subscription object after the move.
         */
        subscription &operator=(subscription &&other) noexcept;

        /**
         * @fn ~subscription
         * @brief Destructor automatically unsubscribes from the event if the subscription is still valid. This ensures that resources are properly released and that the event handler is no longer active when the subscription goes out of scope.
         */
        ~subscription();

        /**
         * @fn reset
         * @brief Unsubscribes from the event and resets the subscription to an invalid state. After calling reset(), the subscription will no longer be valid, and any event handler associated with this subscription will be unsubscribed from the dispatcher. You can check if the subscription is valid after resetting it using the valid() member function.
         */
        void reset() noexcept;

        /**
         * @fn valid
         * @brief Checks if the subscription is currently valid. A subscription is considered valid if it is associated with a dispatcher, has a valid event type ID, and has a non-zero token. If any of these conditions are not met, the subscription is considered invalid.
         * @return true if the subscription is valid; false otherwise.
         */
        [[nodiscard]] bool valid() const noexcept;

    private:
        friend class dispatcher; // Allow dispatcher to access private members for managing subscriptions

        /**
         * @fn subscription
         * @brief Private constructor used by the dispatcher to create a subscription when an event handler is subscribed. This constructor initializes the subscription with the dispatcher pointer, event type ID, token, and active state.
         * @param dispatcher_ptr Pointer to the dispatcher that manages this subscription.
         * @param type_id The unique event type ID associated with this subscription.
         * @param token The unique token that identifies this subscription within the dispatcher.
         * @param active A shared pointer to an atomic boolean that indicates whether this subscription is currently active. This allows the dispatcher to manage the active state of the subscription and ensure that event handlers are not called after they have been unsubscribed.
         */
        subscription(
            dispatcher *dispatcher_ptr,
            event_type_id type_id,
            std::size_t token,
            std::shared_ptr<std::atomic_bool> active) noexcept;

        /**
         * @var m_dispatcher
         * @brief Pointer to the dispatcher that manages this subscription. This is used to call the unsubscribe function when the subscription is reset or destroyed.
         */
        dispatcher *m_dispatcher = nullptr;
        /**
         * @var m_type_id
         * @brief The unique event type ID associated with this subscription. This is used by the dispatcher to identify which event handler to unsubscribe when the subscription is reset or destroyed.
         */
        event_type_id m_type_id = event_base::invalid_type_id();
        /**
         * @var m_token
         * @brief The unique token that identifies this subscription within the dispatcher. This is used by the dispatcher to identify which event handler to unsubscribe when the subscription is reset or destroyed.
         */
        std::size_t m_token = 0u;
        /**
         * @var m_active
         * @brief A shared pointer to an atomic boolean that indicates whether this subscription is currently active. This allows the dispatcher to manage the active state of the subscription and ensure that event handlers are not called after they have been unsubscribed. When the subscription is reset or destroyed, this atomic boolean is set to false to indicate that the subscription is no longer active.
         */
        std::shared_ptr<std::atomic_bool> m_active;
    };

} // namespace catalyst::core