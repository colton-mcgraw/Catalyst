#include "test_common.hpp"

#include <catalyst/core/dispatcher.hpp>
#include <catalyst/core/event.hpp>
#include <catalyst/core/event_sink.hpp>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
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
    return 0;
}
