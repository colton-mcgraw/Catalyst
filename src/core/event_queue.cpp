#include <catalyst/core/event_queue.hpp>

#include <catalyst/core/dispatcher.hpp>

#include <iterator>
#include <utility>

namespace catalyst::core
{
    void event_queue::push(std::unique_ptr<event_base> e)
    {
        if (!e)
            return;

        if (!e->has_timestamp())
            e->stamp();

        m_events.push_back(std::move(e));
    }

    std::size_t event_queue::dispatch_to(dispatcher &d)
    {
        // Take the current batch out of the member so handlers that push during dispatch write to a fresh vector
        // instead of reallocating the one being iterated.
        std::vector<std::unique_ptr<event_base>> batch;
        batch.swap(m_events);

        std::size_t i = 0u;
        try
        {
            for (; i < batch.size(); ++i)
                d.publish(*batch[i]);
        }
        catch (...)
        {
            // Re-queue the undelivered remainder ahead of anything pushed while dispatching.
            m_events.insert(m_events.begin(),
                            std::make_move_iterator(batch.begin() + static_cast<std::ptrdiff_t>(i + 1u)),
                            std::make_move_iterator(batch.end()));
            throw;
        }

        return batch.size();
    }

    void event_queue::clear() noexcept { m_events.clear(); }
    std::size_t event_queue::size() const noexcept { return m_events.size(); }
    bool event_queue::empty() const noexcept { return m_events.empty(); }

} // namespace catalyst::core
