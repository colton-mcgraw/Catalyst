#include <catalyst/core/concurrent_event_queue.hpp>

#include <catalyst/core/dispatcher.hpp>
#include <catalyst/core/event_queue.hpp>

#include <iterator>
#include <utility>

namespace catalyst::core
{
    void concurrent_event_queue::post(std::unique_ptr<event_base> e)
    {
        if (!e)
            return;

        if (!e->has_timestamp())
            e->stamp();

        const std::lock_guard lock(m_mutex);
        m_events.push_back(std::move(e));
    }

    std::vector<std::unique_ptr<event_base>> concurrent_event_queue::take()
    {
        std::vector<std::unique_ptr<event_base>> batch;
        const std::lock_guard lock(m_mutex);
        batch.swap(m_events);
        return batch;
    }

    std::size_t concurrent_event_queue::drain_to(event_queue &q)
    {
        auto batch = take();
        for (auto &e : batch)
            q.push(std::move(e));
        return batch.size();
    }

    std::size_t concurrent_event_queue::drain_to(dispatcher &d)
    {
        auto batch = take();

        std::size_t i = 0u;
        try
        {
            for (; i < batch.size(); ++i)
                d.publish(*batch[i]);
        }
        catch (...)
        {
            const std::lock_guard lock(m_mutex);
            m_events.insert(m_events.begin(),
                            std::make_move_iterator(batch.begin() + static_cast<std::ptrdiff_t>(i + 1u)),
                            std::make_move_iterator(batch.end()));
            throw;
        }

        return batch.size();
    }

    std::size_t concurrent_event_queue::size() const
    {
        const std::lock_guard lock(m_mutex);
        return m_events.size();
    }

} // namespace catalyst::core
