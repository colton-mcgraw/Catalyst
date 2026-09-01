/**
 * @file concurrent_event_queue.hpp
 * @brief Mutex-guarded event mailbox for producers running on threads other than the dispatcher's owner thread.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include "event.hpp"

#include <concepts>
#include <cstddef>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>

namespace catalyst::core
{
	class dispatcher;
	class event_queue;

	/**
	 * @class concurrent_event_queue
	 * @brief The cross-thread entry point into the (single-threaded) event system.
	 *
	 * Any thread may post(). The dispatcher's owner thread periodically drains the mailbox, either into a plain event_queue
	 * or straight into a dispatcher. Handlers always run on the draining thread, never on the posting thread. Events are
	 * timestamped at post time so latency through the mailbox can be measured.
	 */
	class concurrent_event_queue
	{
	public:
		concurrent_event_queue() = default;
		concurrent_event_queue(const concurrent_event_queue &) = delete;
		concurrent_event_queue &operator=(const concurrent_event_queue &) = delete;

		/** @brief Constructs an event of type T and posts it. Thread-safe. */
		template <typename T, typename... Args>
			requires(event_type<T> && std::constructible_from<T, Args...>)
		void post(Args &&...args)
		{
			auto e = std::make_unique<T>(std::forward<Args>(args)...);
			e->stamp();
			post(std::move(e));
		}

		/** @brief Posts an already-constructed event. It is stamped if it has no timestamp yet. Null is ignored. Thread-safe. */
		void post(std::unique_ptr<event_base> e);

		/**
		 * @brief Moves every posted event into a single-threaded queue, preserving order. Call on the owner thread.
		 * @return Number of events moved.
		 */
		std::size_t drain_to(event_queue &q);

		/**
		 * @brief Publishes every posted event directly. Call on the owner thread. Handlers run outside the lock, so they may
		 * post() without deadlocking; such events are delivered on the next drain.
		 * @return Number of events delivered.
		 * @throws Anything a handler throws; the event whose handler threw is dropped and the rest are put back in order.
		 */
		std::size_t drain_to(dispatcher &d);

		/** @brief Number of events currently waiting. Thread-safe, but only a snapshot. */
		[[nodiscard]] std::size_t size() const;

	private:
		std::vector<std::unique_ptr<event_base>> take();

		mutable std::mutex m_mutex;
		std::vector<std::unique_ptr<event_base>> m_events;
	};

} // namespace catalyst::core
