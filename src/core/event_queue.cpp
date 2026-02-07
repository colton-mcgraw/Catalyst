#include <catalyst/core/dispatcher.hpp>
#include <catalyst/core/event_queue.hpp>

namespace catalyst::core
{

    void event_queue::push(std::unique_ptr<event_base> e) { m_events.emplace_back(std::move(e)); }

    void event_queue::dispatch_to(dispatcher &d)
    {
        for (const auto &e : m_events)
            d.publish(*e);
        m_events.clear();
    }

    void event_queue::clear() noexcept { m_events.clear(); }
    std::size_t event_queue::size() const noexcept { return m_events.size(); }

} // namespace catalyst::core