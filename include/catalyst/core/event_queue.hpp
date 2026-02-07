/**
 * @file event_queue.hpp
 * @brief Defines the event_queue class, which is responsible for managing a queue of events and dispatching them to a dispatcher. The event_queue allows clients to push events onto the queue and then dispatch all queued events to a specified dispatcher, which will invoke the appropriate handlers for each event. The event_queue also provides functionality to clear the queue and check the number of events currently in the queue.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include "event.hpp"
#include "subscription.hpp"

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

/**
 * @namespace catalyst::core
 * @brief The catalyst::core namespace contains core functionalities of the Catalyst framework, including event handling and dispatching.
 */
namespace catalyst::core
{
	/**
	 * @class event_queue
	 * @brief Manages a queue of events and dispatches them to a dispatcher. Clients can push events onto the queue and then dispatch all queued events to a specified dispatcher, which will invoke the appropriate handlers for each event. The event_queue also provides functionality to clear the queue and check the number of events currently in the queue.
	 */
	class event_queue
	{
	public:
		/**
		 * @fn event_queue
		 * @brief Default constructor initializes an empty event queue. The event_queue is ready to accept events and dispatch them after construction.
		 */
		event_queue() = default;
		
		/**
		 * @fn push
		 * @brief Pushes a new event onto the queue. This function takes a variable number of arguments that are forwarded to the constructor of the event type. The event type must derive from catalyst::core::event_base. The function creates a new event object using the provided arguments and adds it to the queue for later dispatching.
		 * @tparam T The type of event to push onto the queue. This must derive from catalyst::core::event_base.
		 * @tparam Args The types of the arguments to be forwarded to the constructor of the event type. These arguments are used to construct the event object that will be added to the queue.
		 */
		template <typename T, typename... Args>
			requires(event_type<T> && std::constructible_from<T, Args...>)
		void push(Args &&...args)
		{
			m_events.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
		}

		/**
		 * @fn push
		 * @brief Pushes an existing event object onto the queue. This function takes a unique pointer to an event object and adds it to the queue for later dispatching. The event object must derive from catalyst::core::event_base. This overload allows clients to create event objects separately and then push them onto the queue when they are ready to be dispatched.
		 * @param e A unique pointer to an event object that is to be added to the queue. The event object must derive from catalyst::core::event_base and will be moved into the queue for later dispatching.
		 */
		void push(std::unique_ptr<event_base> e);

		/**
		 * @fn dispatch_to
		 * @brief Dispatches all events currently in the queue to a specified dispatcher. This function iterates through all events in the queue and publishes each event to the provided dispatcher, which will invoke the appropriate handlers for each event type. After dispatching all events, the queue is cleared to prepare for new events to be added.
		 * @param d A reference to a dispatcher object to which all queued events will be published. The dispatcher will handle invoking the appropriate handlers for each event type when the events are published.
		 */
		void dispatch_to(dispatcher &d);

		/**
		 * @fn clear
		 * @brief Clears all events from the queue. This function removes all events currently stored in the queue without dispatching them. After calling this function, the queue will be empty and ready to accept new events.
		 */
		void clear() noexcept;

		/**
		 * @fn size
		 * @brief Returns the number of events currently in the queue. This function provides a way to check how many events are waiting to be dispatched. It returns the size of the internal vector that holds the event objects.
		 * @return The number of events currently in the queue.
		 */
		[[nodiscard]] std::size_t size() const noexcept;

	private:
		/**
		 * @var m_events
		 * @brief A vector that holds unique pointers to event objects currently in the queue. Each event object must derive from catalyst::core::event_base. This vector is used to store events that have been pushed onto the queue and are waiting to be dispatched. When events are dispatched, they are published to the dispatcher and then removed from the queue.
		 */
		std::vector<std::unique_ptr<event_base>> m_events;
	};

} // namespace catalyst::core
