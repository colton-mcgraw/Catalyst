#pragma once

#include "event.hpp"
#include "subscription.hpp"

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace catalyst::core
{

	class event_queue
	{
	public:
		event_queue() = default;

		template <typename Event_T, typename... Args>
		void push(Args &&...args)
		{
			static_assert(std::is_base_of_v<event_base, Event_T>, "Event_T must derive from catalyst::core::event_base");
			m_events.emplace_back(std::make_unique<Event_T>(std::forward<Args>(args)...));
		}

		void push(std::unique_ptr<event_base> e) { m_events.emplace_back(std::move(e)); }

		void dispatch_to(dispatcher &d)
		{
			for (const auto &e : m_events)
				d.publish(*e);
			m_events.clear();
		}

		void clear() noexcept { m_events.clear(); }

		[[nodiscard]] std::size_t size() const noexcept { return m_events.size(); }

	private:
		std::vector<std::unique_ptr<event_base>> m_events;
	};

} // namespace catalyst::core
