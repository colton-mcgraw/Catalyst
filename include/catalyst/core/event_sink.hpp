/**
 * @file event_sink.hpp
 * @brief Thin non-owning facade over a dispatcher, used to hand a publish/subscribe endpoint to code (such as the platform
 * backends) that must not own the dispatcher.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include "dispatcher.hpp"

namespace catalyst::core
{
	/**
	 * @class event_sink
	 * @brief Forwards subscribe/publish to a dispatcher the caller keeps alive. Same threading rules as dispatcher.
	 */
	class event_sink
	{
	public:
		explicit event_sink(dispatcher &dispatcher_ref) noexcept
		: m_dispatcher(&dispatcher_ref) {}

		template <typename T>
		[[nodiscard]] subscription subscribe(void (*callback)(const T &), int priority = 0)
		{
			return m_dispatcher->subscribe<T>(callback, priority);
		}

		template <typename T>
		[[nodiscard]] subscription subscribe(bool (*callback)(const T &), int priority = 0)
		{
			return m_dispatcher->subscribe<T>(callback, priority);
		}

		template <typename T, typename Callback>
			requires(registered_event<T> &&
					 event_callback_for<Callback, T> &&
					 std::copy_constructible<std::decay_t<Callback>> &&
					 (!function_pointer<Callback>))
		[[nodiscard]] subscription subscribe(Callback &&callback, int priority = 0)
		{
			return m_dispatcher->subscribe<T>(std::forward<Callback>(callback), priority);
		}

		/** @return true if a handler consumed the event. */
		bool publish(const event_base &e) { return m_dispatcher->publish(e); }

		template <typename T> requires(registered_event<T>)
		bool publish(const T &e)
		{
			return m_dispatcher->publish<T>(e);
		}

		[[nodiscard]] dispatcher &get_dispatcher() const noexcept { return *m_dispatcher; }

	private:
		dispatcher *m_dispatcher = nullptr;
	};

} // namespace catalyst::core
