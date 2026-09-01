#include "test_common.hpp"

#include <catalyst/core/concurrent_event_queue.hpp>
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

    void test_concurrent_queue_from_many_threads()
    {
        dispatcher d;
        concurrent_event_queue mailbox;
        std::atomic<int> total{0};
        auto sub = d.subscribe<tick>([&](const tick &e) { total += e.value; });

        constexpr int threads = 8;
        constexpr int per_thread = 2000;
        std::vector<std::thread> workers;
        for (int t = 0; t < threads; ++t)
            workers.emplace_back([&]
            {
                for (int i = 0; i < per_thread; ++i)
                    mailbox.post<tick>(1);
            });
        for (auto &w : workers)
            w.join();

        CT_REQUIRE(mailbox.size() == static_cast<std::size_t>(threads * per_thread));

        // Drain path 1: into a single-threaded queue.
        event_queue q;
        CT_REQUIRE(mailbox.drain_to(q) == static_cast<std::size_t>(threads * per_thread));
        CT_REQUIRE(mailbox.size() == 0u);
        q.dispatch_to(d);
        CT_REQUIRE(total == threads * per_thread);

        // Drain path 2: straight into the dispatcher, with a handler that posts back (must not deadlock).
        total = 0;
        auto reposter = d.subscribe<tick>([&](const tick &e)
        {
            if (e.value == 7)
                mailbox.post<tick>(1);
        });
        mailbox.post<tick>(7);
        CT_REQUIRE(mailbox.drain_to(d) == 1u);
        CT_REQUIRE(total == 7);
        CT_REQUIRE(mailbox.size() == 1u);
        CT_REQUIRE(mailbox.drain_to(d) == 1u);
        CT_REQUIRE(total == 8);
    }
} // namespace

int main()
{
    test_push_during_dispatch_is_kept_for_next_batch();
    test_events_are_stamped_on_push();
    test_exception_requeues_remainder();
    test_concurrent_queue_from_many_threads();
    return 0;
}
