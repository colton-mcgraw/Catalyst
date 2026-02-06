#pragma once

#include "dispatcher.hpp"

namespace catalyst::core
{

	class event_sink
	{
	public:
		explicit event_sink(dispatcher &dispatcher_ref) noexcept : m_dispatcher(&dispatcher_ref) {}

		template <typename Event_T>
		[[nodiscard]] subscription subscribe(void (*callback)(const Event_T &))
		{
			return m_dispatcher->subscribe<Event_T>(callback);
		}

		void publish(const event_base &e) { m_dispatcher->publish(e); }

		template <typename Event_T>
		void publish(const Event_T &e)
		{
			m_dispatcher->publish<Event_T>(e);
		}

	private:
		dispatcher *m_dispatcher = nullptr;
	};

} // namespace catalyst::core
