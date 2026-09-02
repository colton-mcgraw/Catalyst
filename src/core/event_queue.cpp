#include <catalyst/core/event_queue.hpp>

#include <catalyst/core/dispatcher.hpp>

#include <iterator>
#include <utility>

namespace catalyst::core
{
    void event_queue::compact_locked()
    {
        if (m_head == 0u)
            return;

        if (m_head == m_events.size())
            m_events.clear();
        else
            m_events.erase(m_events.begin(), m_events.begin() + static_cast<std::ptrdiff_t>(m_head));

        m_head = 0u;
    }

    void event_queue::trim_locked(std::size_t capacity) noexcept
    {
        if (capacity == unbounded)
            return;

        std::size_t dropped = 0u;
        while (live_locked() > capacity)
        {
            m_events[m_head++].reset();
            ++dropped;
        }

        if (dropped != 0u)
        {
            m_dropped.fetch_add(dropped, std::memory_order_relaxed);
            m_size.store(live_locked(), std::memory_order_release);
        }
    }

    void event_queue::push(std::unique_ptr<event_base> e)
    {
        if (!e)
            return;

        if (!e->has_timestamp())
            e->stamp();

        const coalesce_key key = e->get_coalesce_key();
        bool notify = false;

        {
            const std::lock_guard lock(m_mutex);

            // Collapse against the back of the queue when both events describe the same slot of the same stream. Only the
            // back is a candidate: replacing anything further in would let the newer event overtake events it was published
            // after, and the whole point of a queue is that it does not do that.
            if (key != no_coalescing && live_locked() != 0u)
            {
                const event_base &back = *m_events.back();
                if (back.type_id() == e->type_id() && back.get_coalesce_key() == key)
                {
                    m_events.back() = std::move(e);
                    m_coalesced.fetch_add(1u, std::memory_order_relaxed);
                    return; // Size is unchanged, so nothing became available that a waiter was not already free to take.
                }
            }

            // Reclaim the space left by events already popped, but only once enough of the buffer is dead for the move to
            // pay for itself. Halving amortises to O(1) per event.
            if (m_head != 0u && m_head * 2u >= m_events.size())
                compact_locked();

            // Make room before appending rather than after, so the queue never momentarily holds capacity + 1 events.
            if (const std::size_t capacity = m_capacity.load(std::memory_order_relaxed); capacity != unbounded)
                trim_locked(capacity > 0u ? capacity - 1u : 0u);

            m_events.push_back(std::move(e));
            m_size.store(live_locked(), std::memory_order_release);

            notify = m_waiters.load(std::memory_order_relaxed) != 0u;
        }

        if (notify)
            m_ready.notify_one();
    }

    std::vector<std::unique_ptr<event_base>> event_queue::take()
    {
        std::vector<std::unique_ptr<event_base>> batch;

        // Nothing queued: skip the lock entirely. A frame loop calls this every frame whether or not anything happened.
        if (size() == 0u)
            return batch;

        const std::lock_guard lock(m_mutex);
        compact_locked();
        batch.swap(m_events);
        m_size.store(0u, std::memory_order_release);
        return batch;
    }

    bool event_queue::try_pop(std::unique_ptr<event_base> &out)
    {
        if (size() == 0u)
            return false;

        const std::lock_guard lock(m_mutex);
        if (live_locked() == 0u)
            return false;

        out = std::move(m_events[m_head]);
        ++m_head;

        // Fully drained: reset to the front rather than leaving the whole buffer dead behind the head.
        if (m_head == m_events.size())
        {
            m_events.clear();
            m_head = 0u;
        }

        m_size.store(live_locked(), std::memory_order_release);
        return true;
    }

    void event_queue::requeue_front(std::vector<std::unique_ptr<event_base>> &batch, std::size_t from)
    {
        if (from >= batch.size())
            return;

        const std::lock_guard lock(m_mutex);
        compact_locked();
        m_events.insert(m_events.begin(),
                        std::make_move_iterator(batch.begin() + static_cast<std::ptrdiff_t>(from)),
                        std::make_move_iterator(batch.end()));
        m_size.store(live_locked(), std::memory_order_release);
    }

    std::size_t event_queue::dispatch_to(dispatcher &d)
    {
        // Take the current batch out of the queue so handlers that push during dispatch write to the (now empty) queue
        // instead of mutating the container being iterated. This is also what keeps the lock off the handlers: nothing
        // below this point touches the queue unless a handler throws.
        auto batch = take();

        std::size_t i = 0u;
        try
        {
            for (; i < batch.size(); ++i)
                d.publish(*batch[i]);
        }
        catch (...)
        {
            // Re-queue the undelivered remainder ahead of anything pushed while dispatching. The event whose handler threw
            // is dropped: re-delivering it would throw again on the next dispatch and the queue would never drain.
            requeue_front(batch, i + 1u);
            throw;
        }

        return batch.size();
    }

    std::size_t event_queue::drain_to(event_queue &other)
    {
        if (&other == this)
            return 0u;

        auto batch = take();
        const std::size_t moved = batch.size();
        for (auto &e : batch)
            other.push(std::move(e));
        return moved;
    }

    void event_queue::clear() noexcept
    {
        const std::lock_guard lock(m_mutex);
        m_events.clear();
        m_head = 0u;
        m_size.store(0u, std::memory_order_release);
    }

    bool event_queue::wait_for_events(std::chrono::milliseconds timeout)
    {
        std::unique_lock lock(m_mutex);
        if (live_locked() != 0u)
            return true;
        if (timeout <= std::chrono::milliseconds::zero())
            return false;

        const std::uint64_t seq = m_wake_seq;
        m_waiters.fetch_add(1u, std::memory_order_relaxed);
        m_ready.wait_for(lock, timeout, [&] { return live_locked() != 0u || m_wake_seq != seq; });
        m_waiters.fetch_sub(1u, std::memory_order_relaxed);

        return live_locked() != 0u;
    }

    bool event_queue::wait_for_events()
    {
        std::unique_lock lock(m_mutex);
        if (live_locked() != 0u)
            return true;

        const std::uint64_t seq = m_wake_seq;
        m_waiters.fetch_add(1u, std::memory_order_relaxed);
        m_ready.wait(lock, [&] { return live_locked() != 0u || m_wake_seq != seq; });
        m_waiters.fetch_sub(1u, std::memory_order_relaxed);

        return live_locked() != 0u;
    }

    void event_queue::wake()
    {
        {
            const std::lock_guard lock(m_mutex);
            ++m_wake_seq;
        }
        m_ready.notify_all();
    }

    void event_queue::set_capacity(std::size_t max_events)
    {
        const std::lock_guard lock(m_mutex);
        m_capacity.store(max_events, std::memory_order_relaxed);
        trim_locked(max_events);
        compact_locked();
    }

} // namespace catalyst::core
