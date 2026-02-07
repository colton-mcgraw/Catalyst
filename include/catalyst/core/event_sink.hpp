/**
 * @file event_sink.hpp
 * @brief Defines the event_sink class, which provides a convenient interface for subscribing to and publishing events using a dispatcher. The event_sink class acts as a wrapper around a dispatcher, allowing users to easily manage event subscriptions and publish events without directly interacting with the dispatcher. This class is designed to simplify the process of working with events in the Catalyst framework, making it easier for developers to create responsive and event-driven applications. The event_sink class provides methods for subscribing to specific event types and publishing events, while internally managing the underlying dispatcher and subscription resources.
 * @details The event_sink class is part of the catalyst::core namespace and is included in the Catalyst framework. It relies on the dispatcher class for managing event subscriptions and publishing events. The event_sink class provides a user-friendly interface for working with events, allowing developers to focus on their application logic rather than the complexities of event management. By using the event_sink class, developers can easily create responsive applications that react to various events, improving the overall user experience and making it easier to build complex systems.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include "dispatcher.hpp"

/**
 * @namespace catalyst::core
 * @brief The catalyst::core namespace contains the core components of the Catalyst framework, including the event system, which consists of the event_base class, the dispatcher class, the subscription class, and the event_sink class. These components work together to provide a flexible and efficient event-driven architecture for building responsive applications. The event_sink class serves as a convenient interface for subscribing to and publishing events using a dispatcher, allowing developers to easily manage event subscriptions and publish events without directly interacting with the dispatcher. By utilizing the catalyst::core namespace, developers can take advantage of the powerful event system provided by the Catalyst framework to create dynamic and responsive applications.
 */
namespace catalyst::core
{
	/**
	 * @class event_sink
	 * @brief The event_sink class provides a convenient interface for subscribing to and publishing events using a dispatcher. It acts as a wrapper around a dispatcher, allowing users to easily manage event subscriptions and publish events without directly interacting with the dispatcher. The event_sink class simplifies the process of working with events in the Catalyst framework, making it easier for developers to create responsive and event-driven applications.
	 * @details The event_sink class is designed to provide a user-friendly interface for working with events, allowing developers to focus on their application logic rather than the complexities of event management. By using the event_sink class, developers can easily create responsive applications that react to various events, improving the overall user experience and making it easier to build complex systems. The event_sink class provides methods for subscribing to specific event types and publishing events, while internally managing the underlying dispatcher and subscription resources.
	 */
	class event_sink
	{
	public:
		/**
		 * @fn event_sink
		 * @brief Constructs an event_sink that wraps around a given dispatcher. This constructor takes a reference to a dispatcher object and initializes the event_sink to use that dispatcher for managing event subscriptions and publishing events. The event_sink will internally store a pointer to the provided dispatcher, allowing it to forward subscription and publication requests to the dispatcher as needed.
		 * @param dispatcher_ref A reference to a dispatcher object that the event_sink will use for managing event subscriptions and publishing events. The event_sink will store a pointer to this dispatcher and will forward all subscription and publication requests to it.
		 */
		explicit event_sink(dispatcher &dispatcher_ref) noexcept 
		: m_dispatcher(&dispatcher_ref) {}

		/**
		 * @fn subscribe
		 * @brief Subscribes a callback function to a specific event type using the underlying dispatcher. This function allows users to subscribe to events of a particular type by providing a callback function that will be invoked whenever an event of that type is published. The subscription is managed through the underlying dispatcher, and the event_sink provides a convenient interface for subscribing without directly interacting with the dispatcher.
		 * @tparam T The type of event to subscribe to. This must derive from catalyst::core::event_base.
		 * @param callback A pointer to a function that takes a const reference to an event of type T and returns void. This function will be called when an event of the specified type is published.
		 * @return A subscription object that represents the active subscription. You can use this object to unsubscribe from the event later or let it go out of scope to automatically unsubscribe.
		 */
		template <typename T>
		[[nodiscard]] subscription subscribe(void (*callback)(const T &))
		{
			return m_dispatcher->subscribe<T>(callback);
		}

		/**
		 * @fn subscribe
		 * @brief Subscribes a callback function to a specific event type using the underlying dispatcher. This function allows users to subscribe to events of a particular type by providing a callback function that will be invoked whenever an event of that type is published. The subscription is managed through the underlying dispatcher, and the event_sink provides a convenient interface for subscribing without directly interacting with the dispatcher.
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
			return m_dispatcher->subscribe<T>(std::forward<Callback>(callback));
		}

		/**
		 * @fn publish
		 * @brief Publishes an event to the underlying dispatcher. This function allows users to publish events of a particular type by providing an event object. The event will be forwarded to the underlying dispatcher, which will then invoke the appropriate handlers for that event type. The event_sink provides a convenient interface for publishing events without directly interacting with the dispatcher.
		 * @param e A const reference to the event to be published. The event must derive from catalyst::core::event_base and have a valid type ID.
		 */
		void publish(const event_base &e) { m_dispatcher->publish(e); }

		/**
		 * @fn publish
		 * @brief Overload of the publish function that allows publishing an event of a specific type without needing to cast it to event_base. This is a convenience function that forwards the call to the main publish function after ensuring that the event type is valid.
		 * @tparam T The type of event to publish. This must derive from catalyst::core::event_base.
		 * @param e A const reference to the event to be published. The event must derive from catalyst::core::event_base and have a valid type ID.
		 */
		template <typename T> requires(registered_event<T>)
		void publish(const T &e)
		{
			m_dispatcher->publish<T>(e);
		}

	private:
		/**
		 * @var m_dispatcher
		 * @brief A pointer to the dispatcher that the event_sink uses for managing event subscriptions and publishing events. This pointer is initialized in the constructor and is used to forward subscription and publication requests to the underlying dispatcher. The event_sink relies on this dispatcher to handle the actual management of event subscriptions and the invocation of event handlers when events are published.
		 */
		dispatcher *m_dispatcher = nullptr;
	};

} // namespace catalyst::core
