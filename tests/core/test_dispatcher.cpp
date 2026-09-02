#include "test_common.hpp"

#include <catalyst/core/dispatcher.hpp>
#include <catalyst/core/event.hpp>
#include <catalyst/core/event_sink.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <utility>
#include <vector>

using namespace catalyst::core;

namespace
{
    struct ping final : event<ping>
    {
        explicit ping(int v = 0) noexcept : value(v) {}
        int value;
    };

    struct pong final : event<pong>
    {
    };

    int g_free_function_calls = 0;
    void free_handler(const ping &) { ++g_free_function_calls; }
    bool consuming_free_handler(const ping &) { return true; }

    void test_basic_delivery_and_reset()
    {
        dispatcher d;
        int calls = 0;
        auto sub = d.subscribe<ping>([&](const ping &e) { calls += e.value; });

        CT_REQUIRE(d.handler_count<ping>() == 1u);
        CT_REQUIRE(!d.publish(ping{2}));
        CT_REQUIRE(calls == 2);

        d.publish(pong{}); // no handlers, must be a no-op
        CT_REQUIRE(calls == 2);

        sub.reset();
        CT_REQUIRE(!sub.valid());
        CT_REQUIRE(d.handler_count<ping>() == 0u);
        d.publish(ping{5});
        CT_REQUIRE(calls == 2);

        sub.reset(); // idempotent
    }

    void test_free_function_overloads()
    {
        dispatcher d;
        g_free_function_calls = 0;
        auto a = d.subscribe<ping>(free_handler);
        auto b = d.subscribe(&free_handler); // T deduced from the pointer
        d.publish(ping{});
        CT_REQUIRE(g_free_function_calls == 2);

        auto c = d.subscribe<ping>(consuming_free_handler, 10);
        CT_REQUIRE(d.publish(ping{}));
        CT_REQUIRE(g_free_function_calls == 2); // consumed before the lower-priority handlers ran
    }

    void test_subscribe_inside_handler_same_type()
    {
        dispatcher d;
        std::vector<subscription> subs;
        int nested_calls = 0;

        subs.push_back(d.subscribe<ping>([&](const ping &)
        {
            // Would reallocate the vector being iterated in a naive implementation.
            for (int i = 0; i < 64; ++i)
                subs.push_back(d.subscribe<ping>([&](const ping &) { ++nested_calls; }));
        }));

        d.publish(ping{});
        CT_REQUIRE(nested_calls == 0); // handlers added mid-dispatch do not see the current event
        CT_REQUIRE(d.handler_count<ping>() == 65u);

        subs.erase(subs.begin()); // drop the spawner so it does not add 64 more
        d.publish(ping{});
        CT_REQUIRE(nested_calls == 64);
    }

    void test_subscribe_and_reset_inside_same_handler()
    {
        dispatcher d;
        int calls = 0;
        auto outer = d.subscribe<ping>([&](const ping &)
        {
            auto tmp = d.subscribe<ping>([&](const ping &) { ++calls; });
            CT_REQUIRE(d.handler_count<ping>() == 2u);
            tmp.reset(); // removed from the pending list before it is ever merged
            CT_REQUIRE(d.handler_count<ping>() == 1u);
        });

        d.publish(ping{});
        d.publish(ping{});
        CT_REQUIRE(calls == 0);
        CT_REQUIRE(d.handler_count<ping>() == 1u);
    }

    void test_unsubscribe_inside_handler()
    {
        dispatcher d;
        int a_calls = 0, b_calls = 0, c_calls = 0;
        subscription a, b, c;

        a = d.subscribe<ping>([&](const ping &)
        {
            ++a_calls;
            a.reset(); // unsubscribe self
            b.reset(); // unsubscribe a later handler: it must not run for this event
        });
        b = d.subscribe<ping>([&](const ping &) { ++b_calls; });
        c = d.subscribe<ping>([&](const ping &) { ++c_calls; });

        d.publish(ping{});
        CT_REQUIRE(a_calls == 1);
        CT_REQUIRE(b_calls == 0);
        CT_REQUIRE(c_calls == 1);
        CT_REQUIRE(d.handler_count<ping>() == 1u); // compacted after the dispatch returned

        d.publish(ping{});
        CT_REQUIRE(a_calls == 1);
        CT_REQUIRE(c_calls == 2);
    }

    void test_dispatcher_destroyed_before_subscription()
    {
        auto d = std::make_unique<dispatcher>();
        auto sub = d->subscribe<ping>([](const ping &) {});
        CT_REQUIRE(sub.valid());

        d.reset();
        CT_REQUIRE(!sub.valid());
        sub.reset(); // must not touch freed memory
    }

    void test_exception_in_handler_keeps_dispatcher_consistent()
    {
        dispatcher d;
        int after_calls = 0;

        auto thrower = d.subscribe<ping>([](const ping &) { throw std::runtime_error("boom"); }, 1);
        auto after = d.subscribe<ping>([&](const ping &) { ++after_calls; }, 0);

        bool caught = false;
        try
        {
            d.publish(ping{});
        }
        catch (const std::runtime_error &)
        {
            caught = true;
        }
        CT_REQUIRE(caught);
        CT_REQUIRE(after_calls == 0); // propagation stopped at the throw

        thrower.reset();

        // Churn must still be compacted: depth counter was restored by the guard.
        for (int i = 0; i < 1000; ++i)
        {
            auto tmp = d.subscribe<ping>([](const ping &) {});
        }
        CT_REQUIRE(d.handler_count<ping>() == 1u);

        d.publish(ping{});
        CT_REQUIRE(after_calls == 1);
    }

    void test_priority_order_and_stability()
    {
        dispatcher d;
        std::string order;

        auto low = d.subscribe<ping>([&](const ping &) { order += 'L'; }, -5);
        auto mid1 = d.subscribe<ping>([&](const ping &) { order += 'a'; }, 0);
        auto high = d.subscribe<ping>([&](const ping &) { order += 'H'; }, 100);
        auto mid2 = d.subscribe<ping>([&](const ping &) { order += 'b'; }, 0);
        auto mid3 = d.subscribe<ping>([&](const ping &) { order += 'c'; }, 0);

        d.publish(ping{});
        CT_REQUIRE(order == "HabcL");

        // Handlers added during a dispatch are merged with the same ordering rules.
        order.clear();
        subscription late;
        auto adder = d.subscribe<ping>([&](const ping &)
        {
            if (!late.valid())
                late = d.subscribe<ping>([&](const ping &) { order += 'X'; }, 50);
        }, 1000);

        d.publish(ping{});
        CT_REQUIRE(order == "HabcL");
        order.clear();
        d.publish(ping{});
        CT_REQUIRE(order == "HXabcL");
    }

    void test_consumption()
    {
        dispatcher d;
        int low_calls = 0;

        auto low = d.subscribe<ping>([&](const ping &) { ++low_calls; }, 0);
        auto ui = d.subscribe<ping>([](const ping &e) { return e.value > 10; }, 10);

        CT_REQUIRE(!d.publish(ping{1}));
        CT_REQUIRE(low_calls == 1);

        CT_REQUIRE(d.publish(ping{11}));
        CT_REQUIRE(low_calls == 1);

        // Any bool-convertible return works.
        auto intret = d.subscribe<ping>([](const ping &) -> int { return 1; }, 20);
        CT_REQUIRE(d.publish(ping{1}));
        CT_REQUIRE(low_calls == 1);
    }

    void test_nested_publish()
    {
        dispatcher d;
        int ping_calls = 0, pong_calls = 0;

        auto a = d.subscribe<ping>([&](const ping &e)
        {
            ++ping_calls;
            d.publish(pong{});
            if (e.value > 0)
                d.publish(ping{e.value - 1}); // recursive same-type publish
        });
        auto b = d.subscribe<pong>([&](const pong &) { ++pong_calls; });

        d.publish(ping{3});
        CT_REQUIRE(ping_calls == 4);
        CT_REQUIRE(pong_calls == 4);
    }

    void test_move_semantics()
    {
        dispatcher d1;
        int calls = 0;
        auto sub = d1.subscribe<ping>([&](const ping &) { ++calls; });

        dispatcher d2 = std::move(d1);
        d2.publish(ping{});
        CT_REQUIRE(calls == 1);
        CT_REQUIRE(sub.valid());

        subscription moved = std::move(sub);
        CT_REQUIRE(!sub.valid());
        CT_REQUIRE(moved.valid());
        d2.publish(ping{});
        CT_REQUIRE(calls == 2);

        moved.reset();
        d2.publish(ping{});
        CT_REQUIRE(calls == 2);
    }

    void test_event_sink_forwarding()
    {
        dispatcher d;
        event_sink sink(d);
        int calls = 0;
        auto sub = sink.subscribe<ping>([&](const ping &) { ++calls; return true; }, 5);
        CT_REQUIRE(sink.publish(ping{}));
        CT_REQUIRE(calls == 1);
        CT_REQUIRE(&sink.get_dispatcher() == &d);
    }

    void test_many_types_isolated()
    {
        // Publishing one type must not touch handler lists of other types (the old global cleanup did).
        dispatcher d;
        std::vector<subscription> subs;
        int pong_calls = 0;
        for (int i = 0; i < 100; ++i)
            subs.push_back(d.subscribe<pong>([&](const pong &) { ++pong_calls; }));

        auto p = d.subscribe<ping>([](const ping &) {});
        for (int i = 0; i < 1000; ++i)
            d.publish(ping{});

        CT_REQUIRE(d.handler_count<pong>() == 100u);
        d.publish(pong{});
        CT_REQUIRE(pong_calls == 100);
    }

    void test_concurrent_publish()
    {
        // Publishes from many threads run at once. The handler is the only thing that needs to be thread-safe.
        dispatcher d;
        std::atomic<int> calls{0};
        auto sub = d.subscribe<ping>([&](const ping &e) { calls.fetch_add(e.value, std::memory_order_relaxed); });

        constexpr int threads = 8;
        constexpr int per_thread = 10'000;
        std::vector<std::thread> workers;
        for (int t = 0; t < threads; ++t)
            workers.emplace_back([&] { for (int i = 0; i < per_thread; ++i) d.publish(ping{1}); });
        for (auto &w : workers)
            w.join();

        CT_REQUIRE(calls.load() == threads * per_thread);
    }

    void test_subscription_churn_during_concurrent_publish()
    {
        // Subscribing and unsubscribing while other threads publish the same event type. Every handler that is live for
        // the whole run must see every event, and a publisher must never observe a half-built or freed handler table.
        dispatcher d;
        std::atomic<int> stable_calls{0};
        std::atomic<int> published{0};
        auto stable = d.subscribe<ping>([&](const ping &) { stable_calls.fetch_add(1, std::memory_order_relaxed); });

        std::atomic<bool> running{true};
        std::vector<std::thread> workers;

        for (int t = 0; t < 4; ++t)
            workers.emplace_back([&]
            {
                while (running.load(std::memory_order_acquire))
                {
                    d.publish(ping{1});
                    published.fetch_add(1, std::memory_order_relaxed);
                }
            });

        for (int t = 0; t < 2; ++t)
            workers.emplace_back([&]
            {
                for (int i = 0; i < 2000; ++i)
                {
                    auto transient = d.subscribe<ping>([](const ping &) {}, i % 3);
                    (void)d.handler_count<ping>();
                }
            });

        for (std::size_t t = 4; t < workers.size(); ++t)
            workers[t].join();
        running.store(false, std::memory_order_release);
        for (std::size_t t = 0; t < 4; ++t)
            workers[t].join();

        CT_REQUIRE(stable_calls.load() == published.load());
        CT_REQUIRE(d.handler_count<ping>() == 1u); // only the stable handler is left; the churn cleaned itself up
    }

    void test_unsubscribe_from_another_thread_stops_delivery()
    {
        dispatcher d;
        std::atomic<int> calls{0};
        std::atomic<bool> stop{false};
        auto sub = d.subscribe<ping>([&](const ping &) { calls.fetch_add(1, std::memory_order_relaxed); });

        std::thread publisher([&]
        {
            while (!stop.load(std::memory_order_acquire))
                d.publish(ping{});
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        sub.reset();
        stop.store(true, std::memory_order_release);
        publisher.join();

        // What reset() guarantees across threads is that no *further* dispatch will select the handler - not that a
        // dispatch already between the flag test and the call will be stopped, which would need reset() to wait for
        // concurrent dispatches to finish. So the assertion is that delivery has stopped once the publishers are quiet.
        const int settled = calls.load();
        for (int i = 0; i < 1000; ++i)
            d.publish(ping{});

        CT_REQUIRE(calls.load() == settled);
        CT_REQUIRE(d.handler_count<ping>() == 0u);
    }

    void test_handler_added_by_another_thread_is_eventually_seen()
    {
        dispatcher d;
        std::atomic<int> late_calls{0};

        std::atomic<bool> stop{false};
        std::thread publisher([&] { while (!stop.load(std::memory_order_acquire)) d.publish(ping{}); });

        auto late = d.subscribe<ping>([&](const ping &) { late_calls.fetch_add(1, std::memory_order_relaxed); });
        while (late_calls.load() == 0)
            std::this_thread::yield();

        stop.store(true, std::memory_order_release);
        publisher.join();
        CT_REQUIRE(late_calls.load() > 0);
    }
} // namespace


int main()
{
    test_basic_delivery_and_reset();
    test_free_function_overloads();
    test_subscribe_inside_handler_same_type();
    test_subscribe_and_reset_inside_same_handler();
    test_unsubscribe_inside_handler();
    test_dispatcher_destroyed_before_subscription();
    test_exception_in_handler_keeps_dispatcher_consistent();
    test_priority_order_and_stability();
    test_consumption();
    test_nested_publish();
    test_move_semantics();
    test_event_sink_forwarding();
    test_many_types_isolated();
    test_concurrent_publish();
    test_subscription_churn_during_concurrent_publish();
    test_unsubscribe_from_another_thread_stops_delivery();
    test_handler_added_by_another_thread_is_eventually_seen();
    return 0;
}
