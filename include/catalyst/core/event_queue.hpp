/**
 * @file event_queue.hpp
 * @brief A thread-safe FIFO of heap-allocated events that can be drained one at a time or delivered to a dispatcher in one
 * batch. Any number of threads may produce; any number may consume.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include "event.hpp"

#include <atomic>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
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
	 * Threading: every member is safe to call from any thread, concurrently, with no external synchronisation. Producers
	 * take a mutex only for as long as it takes to append one pointer, and a consumer takes the whole pending batch out in
	 * one operation rather than popping under the lock repeatedly, so producers and consumers barely contend even at high
	 * event rates. Handlers always run on the thread that drains, never on the thread that pushed.
	 *
	 * dispatch_to() delivers the events that were queued *before* the call. Events pushed by handlers while a dispatch is
	 * running are kept for the next dispatch_to() - they are never dropped and never cause the current batch to be mutated
	 * under the iterator. Call dispatch_to() in a loop (`while (q.dispatch_to(d) != 0) {}`) if same-frame delivery of
	 * follow-up events is wanted.
	 *
	 * Two producers on different threads have no defined order relative to one another, only relative to themselves: events
	 * pushed by one thread are always delivered in the order that thread pushed them. Two consumers draining concurrently
	 * each take a disjoint batch, so an event is delivered exactly once, but the two batches then run in parallel and their
	 * relative order is whatever the scheduler decides. A single consumer is the usual arrangement and the only one that
	 * gives a total order.
	 *
	 * @par Bounding
	 * The queue can be given a capacity, past which pushing discards the *oldest* event rather than refusing the newest and
	 * increments dropped_count(). This matters wherever a producer cannot be throttled and the consumer can be stalled - an
	 * operating system's modal resize loop, for instance, keeps generating input while denying the application any chance to
	 * drain - because the alternative is unbounded allocation, and what the consumer needs when it regains control is the
	 * current state of the world rather than the beginning of a backlog it will immediately discard anyway. The default is
	 * unbounded; see set_capacity.
	 *
	 * @par Coalescing
	 * An event that carries a non-zero coalesce_key replaces the event already at the back of the queue when that event has
	 * the same type id and the same key, instead of being appended after it. This collapses a stream of events that carry a
	 * current value rather than an occurrence (a window position, a window size) down to its newest member, which is the
	 * only one that carries information. Only the back is considered, so the order of everything else in the queue is
	 * untouched and an event can never overtake one it was published after. See coalesce_key for the rules on which event
	 * types may opt in.
	 */
	class event_queue
	{
	public:
		/** @brief The capacity value meaning "no bound". */
		static constexpr std::size_t unbounded = 0u;

		event_queue() = default;

		/** @brief Constructs a queue bounded to @p capacity events (see set_capacity), or unbounded for 0. */
		explicit event_queue(std::size_t capacity) noexcept : m_capacity(capacity) {}

		event_queue(const event_queue &) = delete;
		event_queue &operator=(const event_queue &) = delete;

		/** @brief Constructs an event of type T in place and queues it. Thread-safe. */
		template <typename T, typename... Args>
			requires(event_type<T> && std::constructible_from<T, Args...>)
		void push(Args &&...args)
		{
			auto e = std::make_unique<T>(std::forward<Args>(args)...);
			e->stamp();
			push(std::move(e));
		}

		/**
		 * @brief Constructs an event of type T in place, marks it coalescible within @p key, and queues it. Thread-safe.
		 * @details Equivalent to constructing the event, calling set_coalesce_key on it and pushing it. See coalesce_key for
		 * which events may be collapsed this way - the short version is those that carry a current value rather than an
		 * occurrence.
		 */
		template <typename T, typename... Args>
			requires(event_type<T> && std::constructible_from<T, Args...>)
		void push_coalescing(coalesce_key key, Args &&...args)
		{
			auto e = std::make_unique<T>(std::forward<Args>(args)...);
			e->stamp();
			e->set_coalesce_key(key);
			push(std::move(e));
		}

		/**
		 * @brief Queues an already-constructed event. It is stamped if it does not carry a timestamp yet. Null is ignored.
		 * Thread-safe.
		 */
		void push(std::unique_ptr<event_base> e);

		/** @brief post() is push(). Retained for the code that used the old concurrent_event_queue spelling. */
		template <typename T, typename... Args>
			requires(event_type<T> && std::constructible_from<T, Args...>)
		void post(Args &&...args)
		{
			push<T>(std::forward<Args>(args)...);
		}

		/** @brief post() is push(). Retained for the code that used the old concurrent_event_queue spelling. */
		void post(std::unique_ptr<event_base> e) { push(std::move(e)); }

		/**
		 * @brief Publishes every event that was queued before this call, in order, then releases them.
		 * @return The number of events delivered.
		 * @throws Anything a handler throws. The event whose handler threw is dropped; the remaining events of the batch are
		 * put back at the front of the queue so nothing else is lost. A batch put back this way may exceed the capacity
		 * until the next push trims it.
		 * @note Handlers run outside the queue's lock, so they may push (including from other threads) without deadlocking;
		 * such events are delivered by a later call.
		 */
		std::size_t dispatch_to(dispatcher &d);

		/** @brief drain_to() is dispatch_to(). Retained for the code that used the old concurrent_event_queue spelling. */
		std::size_t drain_to(dispatcher &d) { return dispatch_to(d); }

		/**
		 * @brief Moves every queued event into another queue, preserving order.
		 * @return The number of events moved.
		 */
		std::size_t drain_to(event_queue &other);

		/**
		 * @brief Removes the oldest event, if there is one.
		 * @param out Receives the event. Left untouched when the queue is empty.
		 * @return True if an event was handed over.
		 */
		bool try_pop(std::unique_ptr<event_base> &out);

		/** @brief Takes the whole pending batch out of the queue in one operation, in order. */
		[[nodiscard]] std::vector<std::unique_ptr<event_base>> take();

		/** @brief Discards all queued events. Does not count them as dropped. */
		void clear() noexcept;

		/**
		 * @brief Blocks until the queue is non-empty, wake() is called, or @p timeout elapses.
		 * @param timeout How long to wait at most. A zero timeout tests the queue and returns immediately.
		 * @return True if the queue was non-empty when the wait ended.
		 */
		bool wait_for_events(std::chrono::milliseconds timeout);

		/**
		 * @brief Blocks until the queue is non-empty or wake() is called.
		 * @return True if the queue was non-empty when the wait ended (false only if wake() ended the wait).
		 */
		bool wait_for_events();

		/**
		 * @brief Releases every thread currently blocked in wait_for_events, and makes the next wait_for_events on each of
		 * them return immediately. Used to shut a consumer thread down without pushing a sentinel event.
		 */
		void wake();

		/**
		 * @brief Sets the maximum number of events the queue holds before pushing starts discarding the oldest.
		 * @param max_events The bound, or event_queue::unbounded (0) to remove it. Lowering it below the current size
		 * discards the oldest events immediately and counts them as dropped.
		 */
		void set_capacity(std::size_t max_events);

		/** @brief The capacity set with set_capacity, or event_queue::unbounded (0) if the queue is not bounded. */
		[[nodiscard]] std::size_t capacity() const noexcept { return m_capacity.load(std::memory_order_relaxed); }

		/**
		 * @brief How many events this queue has discarded because it was full, since it was constructed. A non-zero and
		 * growing value means the consumer is not draining often enough, or the capacity is too small for the rate.
		 */
		[[nodiscard]] std::size_t dropped_count() const noexcept { return m_dropped.load(std::memory_order_relaxed); }

		/** @brief How many events this queue has collapsed into a newer one of the same type and coalesce key. */
		[[nodiscard]] std::size_t coalesced_count() const noexcept { return m_coalesced.load(std::memory_order_relaxed); }

		/**
		 * @brief The number of events waiting. Lock-free, but only ever a snapshot: another thread may push or drain the
		 * moment it is read.
		 */
		[[nodiscard]] std::size_t size() const noexcept { return m_size.load(std::memory_order_acquire); }

		/** @brief Whether no events are waiting. The same snapshot caveat as size() applies. */
		[[nodiscard]] bool empty() const noexcept { return size() == 0u; }

	private:
		/** @brief Puts an undelivered batch back at the front of the queue, ahead of anything pushed since it was taken. */
		void requeue_front(std::vector<std::unique_ptr<event_base>> &batch, std::size_t from);

		/** @brief Drops from the front until the size is within @p capacity. Called with m_mutex held. */
		void trim_locked(std::size_t capacity) noexcept;

		/** @brief Moves the live events back to the front of the buffer, so m_head becomes 0. Called with m_mutex held. */
		void compact_locked();

		/** @brief The number of live events. Called with m_mutex held. */
		[[nodiscard]] std::size_t live_locked() const noexcept { return m_events.size() - m_head; }

		mutable std::mutex m_mutex;
		std::condition_variable m_ready;

		/**
		 * @var m_events
		 * @brief The buffer. Live events are [m_head, size); everything before m_head has been popped and is waiting to be
		 * reclaimed.
		 * @details A vector with a head index rather than a deque, because the two operations that make this queue what it
		 * is both want it. Popping the oldest event - which the drop-oldest capacity policy does in bulk, and try_pop does
		 * one at a time - is a single index increment, and taking a whole batch is a single pointer swap that hands the
		 * buffer over without touching the events at all. A deque would allocate and free its block map around every one of
		 * those batches. The dead prefix is reclaimed once it is half the buffer, which amortises to O(1) per event.
		 */
		std::vector<std::unique_ptr<event_base>> m_events;
		std::size_t m_head = 0u;

		/**
		 * @var m_size
		 * @brief A lock-free mirror of m_events.size(), so size() and empty() - which a consumer polls far more often than
		 * it drains - never contend with the producers for the mutex.
		 */
		std::atomic<std::size_t> m_size{0u};

		std::atomic<std::size_t> m_capacity{unbounded};
		std::atomic<std::size_t> m_dropped{0u};
		std::atomic<std::size_t> m_coalesced{0u};

		/**
		 * @var m_waiters
		 * @brief How many threads are blocked in wait_for_events. Producers skip notifying the condition variable entirely
		 * when this is zero, which is the case for every queue that is drained by polling rather than by waiting.
		 */
		std::atomic<std::size_t> m_waiters{0u};

		/**
		 * @var m_wake_seq
		 * @brief Incremented by wake(). A waiter that observes a different value than the one it started with returns even
		 * though the queue is empty, which is what stops a wake() racing with the start of a wait from being lost.
		 */
		std::uint64_t m_wake_seq = 0u;
	};

} // namespace catalyst::core
