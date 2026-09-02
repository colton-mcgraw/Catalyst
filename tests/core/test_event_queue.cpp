#include "test_common.hpp"

#include <catalyst/core/dispatcher.hpp>
#include <catalyst/core/event.hpp>
#include <catalyst/core/event_queue.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace catalyst::core;

namespace
{
    struct tick final : event<tick>
    {
        explicit tick(int v = 0) noexcept : value(v) {}
        int value;
    };

    struct tock final : event<tock>
    {
        explicit tock(int v = 0) noexcept : value(v) {}
        int value;
    };

    void test_push_during_dispatch_is_kept_for_next_batch()
    {
        dispatcher d;
        event_queue q;
        int delivered = 0;

        auto sub = d.subscribe<tick>([&](const tick &e)
        {
            ++delivered;
            if (e.value < 3)
                for (int i = 0; i < 2; ++i)
                    q.push<tick>(e.value + 1); // would reallocate under the old iteration and then be dropped by clear()
        });

        q.push<tick>(0);
        CT_REQUIRE(q.dispatch_to(d) == 1u);
        CT_REQUIRE(delivered == 1);
        CT_REQUIRE(q.size() == 2u); // follow-ups kept

        CT_REQUIRE(q.dispatch_to(d) == 2u);
        CT_REQUIRE(delivered == 3);
        CT_REQUIRE(q.size() == 4u);

        while (q.dispatch_to(d) != 0u)
        {
        }
        CT_REQUIRE(delivered == 15); // 1 + 2 + 4 + 8
        CT_REQUIRE(q.empty());
    }

    void test_events_are_stamped_on_push()
    {
        dispatcher d;
        event_queue q;
        bool stamped = false;
        auto sub = d.subscribe<tick>([&](const tick &e) { stamped = e.has_timestamp(); });

        const tick immediate{};
        CT_REQUIRE(!immediate.has_timestamp()); // immediate publish never pays for the clock

        q.push<tick>();
        q.dispatch_to(d);
        CT_REQUIRE(stamped);

        auto pre = std::make_unique<tick>();
        const auto fixed = event_base::timestamp_t{} + std::chrono::seconds(42);
        pre->set_timestamp(fixed);
        q.push(std::move(pre));
        event_base::timestamp_t seen{};
        auto sub2 = d.subscribe<tick>([&](const tick &e) { seen = e.get_timestamp(); }, 1);
        q.dispatch_to(d);
        CT_REQUIRE(seen == fixed); // an existing timestamp is preserved
    }

    void test_exception_requeues_remainder()
    {
        dispatcher d;
        event_queue q;
        std::vector<int> seen;
        auto sub = d.subscribe<tick>([&](const tick &e)
        {
            if (e.value == 2)
                throw std::runtime_error("boom");
            seen.push_back(e.value);
        });

        for (int i = 1; i <= 4; ++i)
            q.push<tick>(i);

        bool caught = false;
        try
        {
            q.dispatch_to(d);
        }
        catch (const std::runtime_error &)
        {
            caught = true;
        }
        CT_REQUIRE(caught);
        CT_REQUIRE(seen.size() == 1u && seen[0] == 1);
        CT_REQUIRE(q.size() == 2u); // 3 and 4 were put back; 2 (the thrower) was dropped

        q.dispatch_to(d);
        CT_REQUIRE(seen.size() == 3u && seen[1] == 3 && seen[2] == 4);
    }

    void test_try_pop_is_fifo()
    {
        event_queue q;
        std::unique_ptr<event_base> out;

        CT_REQUIRE(!q.try_pop(out));
        CT_REQUIRE(out == nullptr); // a failed pop leaves the destination alone

        for (int i = 0; i < 3; ++i)
            q.push<tick>(i);

        for (int i = 0; i < 3; ++i)
        {
            CT_REQUIRE(q.try_pop(out));
            CT_REQUIRE(static_cast<const tick &>(*out).value == i);
        }
        CT_REQUIRE(!q.try_pop(out));
    }

    void test_capacity_drops_the_oldest()
    {
        event_queue q(4u);
        CT_REQUIRE(q.capacity() == 4u);

        for (int i = 0; i < 10; ++i)
            q.push<tick>(i);

        CT_REQUIRE(q.size() == 4u);
        CT_REQUIRE(q.dropped_count() == 6u);

        // What survives is the newest state, not the beginning of a backlog.
        std::unique_ptr<event_base> out;
        for (int i = 6; i < 10; ++i)
        {
            CT_REQUIRE(q.try_pop(out));
            CT_REQUIRE(static_cast<const tick &>(*out).value == i);
        }

        // Lowering the bound trims immediately; raising it never drops anything.
        for (int i = 0; i < 4; ++i)
            q.push<tick>(i);
        q.set_capacity(2u);
        CT_REQUIRE(q.size() == 2u);
        CT_REQUIRE(q.dropped_count() == 8u);

        q.set_capacity(event_queue::unbounded);
        CT_REQUIRE(q.capacity() == event_queue::unbounded);
        for (int i = 0; i < 1000; ++i)
            q.push<tick>(i);
        CT_REQUIRE(q.size() == 1002u);
        CT_REQUIRE(q.dropped_count() == 8u);
    }

    void test_coalescing_collapses_the_tail_only()
    {
        event_queue q;

        // No key: nothing is collapsed, however redundant the events look.
        for (int i = 0; i < 5; ++i)
            q.push<tick>(i);
        CT_REQUIRE(q.size() == 5u);
        CT_REQUIRE(q.coalesced_count() == 0u);
        q.clear();

        // Same type, same key: only the newest survives.
        for (int i = 0; i < 5; ++i)
            q.push_coalescing<tick>(7u, i);
        CT_REQUIRE(q.size() == 1u);
        CT_REQUIRE(q.coalesced_count() == 4u);

        std::unique_ptr<event_base> out;
        CT_REQUIRE(q.try_pop(out));
        CT_REQUIRE(static_cast<const tick &>(*out).value == 4);

        // Different keys are different streams and never merge.
        q.push_coalescing<tick>(1u, 10);
        q.push_coalescing<tick>(2u, 20);
        q.push_coalescing<tick>(1u, 11);
        CT_REQUIRE(q.size() == 3u); // the second key-1 event is not at the tail, so it is appended

        q.clear();

        // A matching key on a different event type is a different stream too.
        q.push_coalescing<tick>(1u, 1);
        q.push_coalescing<tock>(1u, 2);
        CT_REQUIRE(q.size() == 2u);

        q.clear();

        // Coalescing never lets an event overtake one published before it.
        q.push_coalescing<tick>(1u, 1);
        q.push<tock>(99);
        q.push_coalescing<tick>(1u, 2);
        CT_REQUIRE(q.size() == 3u);
        CT_REQUIRE(q.try_pop(out) && out->type_id() == tick::type_id());
        CT_REQUIRE(q.try_pop(out) && out->type_id() == tock::type_id());
        CT_REQUIRE(q.try_pop(out) && static_cast<const tick &>(*out).value == 2);
    }

    void test_coalescing_keeps_a_bounded_queue_useful()
    {
        // The case this exists for: a producer that cannot be throttled emits one value event per pixel of mouse travel
        // while the consumer has no opportunity to drain. Without coalescing the bound would be spent entirely on stale
        // positions of one stream and the unrelated events would be dropped.
        event_queue q(8u);

        q.push<tock>(1);
        for (int i = 0; i < 10'000; ++i)
            q.push_coalescing<tick>(1u, i);

        CT_REQUIRE(q.size() == 2u);
        CT_REQUIRE(q.dropped_count() == 0u); // the bound was never reached

        std::unique_ptr<event_base> out;
        CT_REQUIRE(q.try_pop(out) && out->type_id() == tock::type_id());
        CT_REQUIRE(q.try_pop(out) && static_cast<const tick &>(*out).value == 9'999);
    }

    void test_many_producers_one_consumer()
    {
        dispatcher d;
        event_queue q;
        std::atomic<int> total{0};
        auto sub = d.subscribe<tick>([&](const tick &e) { total += e.value; });

        constexpr int threads = 8;
        constexpr int per_thread = 2000;
        std::vector<std::thread> workers;
        for (int t = 0; t < threads; ++t)
            workers.emplace_back([&]
            {
                for (int i = 0; i < per_thread; ++i)
                    q.push<tick>(1);
            });
        for (auto &w : workers)
            w.join();

        CT_REQUIRE(q.size() == static_cast<std::size_t>(threads * per_thread));

        // Drain path 1: into another queue.
        event_queue other;
        CT_REQUIRE(q.drain_to(other) == static_cast<std::size_t>(threads * per_thread));
        CT_REQUIRE(q.size() == 0u);
        other.dispatch_to(d);
        CT_REQUIRE(total == threads * per_thread);

        // Drain path 2: straight into the dispatcher, with a handler that pushes back (must not deadlock).
        total = 0;
        auto repusher = d.subscribe<tick>([&](const tick &e)
        {
            if (e.value == 7)
                q.push<tick>(1);
        });
        q.push<tick>(7);
        CT_REQUIRE(q.dispatch_to(d) == 1u);
        CT_REQUIRE(total == 7);
        CT_REQUIRE(q.size() == 1u);
        CT_REQUIRE(q.dispatch_to(d) == 1u);
        CT_REQUIRE(total == 8);
    }

    void test_producers_and_consumers_overlap()
    {
        // Producers and consumers running at the same time: every event must be delivered exactly once, and a consumer
        // must never observe a torn or partially published event.
        dispatcher d;
        event_queue q;
        std::atomic<int> total{0};
        auto sub = d.subscribe<tick>([&](const tick &e) { total.fetch_add(e.value, std::memory_order_relaxed); });

        constexpr int producers = 4;
        constexpr int consumers = 3;
        constexpr int per_thread = 5000;

        std::atomic<bool> producing{true};
        std::atomic<int> dispatched{0};

        std::vector<std::thread> workers;
        for (int t = 0; t < producers; ++t)
            workers.emplace_back([&]
            {
                for (int i = 0; i < per_thread; ++i)
                    q.push<tick>(1);
            });

        for (int t = 0; t < consumers; ++t)
            workers.emplace_back([&]
            {
                while (producing.load(std::memory_order_acquire) || !q.empty())
                    dispatched.fetch_add(static_cast<int>(q.dispatch_to(d)), std::memory_order_relaxed);
            });

        for (int t = 0; t < producers; ++t)
            workers[static_cast<std::size_t>(t)].join();
        producing.store(false, std::memory_order_release);
        for (std::size_t t = producers; t < workers.size(); ++t)
            workers[t].join();

        CT_REQUIRE(q.empty());
        CT_REQUIRE(dispatched.load() == producers * per_thread);
        CT_REQUIRE(total.load() == producers * per_thread);
    }

    void test_wait_for_events()
    {
        event_queue q;

        // Nothing queued and no producer: the wait times out rather than returning spuriously.
        const auto started = std::chrono::steady_clock::now();
        CT_REQUIRE(!q.wait_for_events(std::chrono::milliseconds(20)));
        CT_REQUIRE(std::chrono::steady_clock::now() - started >= std::chrono::milliseconds(15));

        // Already non-empty: returns without blocking, even with a zero timeout.
        q.push<tick>(1);
        CT_REQUIRE(q.wait_for_events(std::chrono::milliseconds(0)));
        q.clear();

        // A push on another thread releases the waiter.
        std::thread producer([&]
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            q.push<tick>(1);
        });
        CT_REQUIRE(q.wait_for_events());
        CT_REQUIRE(q.size() == 1u);
        producer.join();
        q.clear();

        // wake() releases a waiter with nothing queued, which is how a consumer thread is shut down.
        std::thread waker([&]
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            q.wake();
        });
        CT_REQUIRE(!q.wait_for_events()); // false: released by wake(), not by an event
        waker.join();
    }

} // namespace

int main()
{
    test_push_during_dispatch_is_kept_for_next_batch();
    test_events_are_stamped_on_push();
    test_exception_requeues_remainder();
    test_try_pop_is_fifo();
    test_capacity_drops_the_oldest();
    test_coalescing_collapses_the_tail_only();
    test_coalescing_keeps_a_bounded_queue_useful();
    test_many_producers_one_consumer();
    test_producers_and_consumers_overlap();
    test_wait_for_events();
    return 0;
}
