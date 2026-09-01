/**
 * @file event_queue.hpp
 * @brief Single-threaded FIFO of heap-allocated events that can be delivered to a dispatcher in one batch.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include "event.hpp"

#include <concepts>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace catalyst::core
{
	class dispatcher;

	/**
	 * @class event_queue
	 * @brief Buffers events for deferred delivery. Events are timestamped when pushed.
	 *
	 * dispatch_to() delivers the events that were queued *before* the call. Events pushed by handlers while a dispatch is
	 * running are kept for the next dispatch_to() - they are never dropped and never cause the current batch to reallocate
	 * under the iterator. Call dispatch_to() in a loop (`while (q.dispatch_to(d) != 0) {}`) if same-frame delivery of
	 * follow-up events is wanted.
	 *
	 * Not thread-safe; see concurrent_event_queue for cross-thread producers.
	 */
	class event_queue
	{
	public:
		event_queue() = default;

		/** @brief Constructs an event of type T in place and queues it. */
		template <typename T, typename... Args>
			requires(event_type<T> && std::constructible_from<T, Args...>)
		void push(Args &&...args)
		{
			auto e = std::make_unique<T>(std::forward<Args>(args)...);
			e->stamp();
			m_events.push_back(std::move(e));
		}

		/** @brief Queues an already-constructed event. It is stamped if it does not carry a timestamp yet. Null is ignored. */
		void push(std::unique_ptr<event_base> e);

		/**
		 * @brief Publishes every event that was queued before this call, in order, then releases them.
		 * @return The number of events delivered.
		 * @throws Anything a handler throws. The event whose handler threw is dropped; the remaining events of the batch are
		 * put back at the front of the queue so nothing else is lost.
		 */
		std::size_t dispatch_to(dispatcher &d);

		/** @brief Discards all queued events. */
		void clear() noexcept;

		[[nodiscard]] std::size_t size() const noexcept;
		[[nodiscard]] bool empty() const noexcept;

	private:
		std::vector<std::unique_ptr<event_base>> m_events;
	};

} // namespace catalyst::core
