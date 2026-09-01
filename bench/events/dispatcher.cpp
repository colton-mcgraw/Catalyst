#include <benchmark.hpp>

#include <catalyst/core/dispatcher.hpp>
#include <catalyst/core/event.hpp>
#include <catalyst/core/event_queue.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <utility>
#include <vector>

namespace
{
    struct benchmark_event final : catalyst::core::event<benchmark_event>
    {
        explicit benchmark_event(std::size_t event_value) noexcept : value(event_value) {}

        std::size_t value;
    };

    // Distinct event types that have subscribers but are never published: a large system has hundreds of these.
    template <int N>
    struct idle_event final : catalyst::core::event<idle_event<N>>
    {
    };

    std::atomic_size_t handled_events{0u};

    void handle_event(const benchmark_event& event)
    {
        handled_events.fetch_add(event.value, std::memory_order_relaxed);
    }

    template <int... Is>
    void subscribe_idle(catalyst::core::dispatcher& dispatcher,
                        std::vector<catalyst::core::subscription>& subscriptions,
                        int handlers_per_type,
                        std::integer_sequence<int, Is...>)
    {
        ((void)[&]
        {
            for (int k = 0; k < handlers_per_type; ++k)
                subscriptions.push_back(dispatcher.subscribe<idle_event<Is>>([](const idle_event<Is>&) {}));
        }(), ...);
    }
} // namespace

int main()
{
    constexpr std::size_t iterations = 1'000'000u;

    catalyst::bench::run("steady_clock::now() (cost of one timestamp)", iterations, []
    {
        handled_events.fetch_add(static_cast<std::size_t>(std::chrono::steady_clock::now().time_since_epoch().count() & 1),
                                 std::memory_order_relaxed);
    });

    {
        catalyst::core::dispatcher dispatcher;
        const auto subscription = dispatcher.subscribe<benchmark_event>(handle_event);

        catalyst::bench::run("dispatcher.publish (one subscriber, construct per call)", iterations, [&]
        {
            dispatcher.publish(benchmark_event{1u});
        });

        const benchmark_event prebuilt{1u};
        catalyst::bench::run("dispatcher.publish (one subscriber, prebuilt event)", iterations, [&]
        {
            dispatcher.publish(prebuilt);
        });

        catalyst::bench::run("dispatcher.publish (no subscribers for this type)", iterations, [&]
        {
            dispatcher.publish(idle_event<0>{});
        });
    }

    {
        // Publish cost must not depend on how many *other* types have subscribers.
        catalyst::core::dispatcher dispatcher;
        const auto subscription = dispatcher.subscribe<benchmark_event>(handle_event);
        std::vector<catalyst::core::subscription> idle;
        subscribe_idle(dispatcher, idle, 8, std::make_integer_sequence<int, 256>{});
        const benchmark_event prebuilt{1u};

        catalyst::bench::run("dispatcher.publish (one subscriber + 256 idle types x 8 handlers)", iterations, [&]
        {
            dispatcher.publish(prebuilt);
        });

        catalyst::bench::run("dispatcher.subscribe + unsubscribe (with 256 idle types present)", iterations, [&]
        {
            const auto transient = dispatcher.subscribe<benchmark_event>([](const benchmark_event&) {});
        });
    }

    {
        // Fan-out: one event, many subscribers of the same type.
        catalyst::core::dispatcher dispatcher;
        std::vector<catalyst::core::subscription> fan;
        for (int i = 0; i < 64; ++i)
            fan.push_back(dispatcher.subscribe<benchmark_event>(handle_event));
        const benchmark_event prebuilt{1u};

        catalyst::bench::run("dispatcher.publish (64 subscribers of the same type)", iterations / 10u, [&]
        {
            dispatcher.publish(prebuilt);
        });

        // Consumption: a high-priority handler swallows the event before the 64 others run.
        const auto consumer = dispatcher.subscribe<benchmark_event>([](const benchmark_event&) { return true; }, 100);
        catalyst::bench::run("dispatcher.publish (consumed by first of 65 handlers)", iterations, [&]
        {
            dispatcher.publish(prebuilt);
        });
    }

    {
        // Robustness: after a handler throws, cleanup must keep working.
        catalyst::core::dispatcher dispatcher;
        const auto subscription = dispatcher.subscribe<benchmark_event>(handle_event);
        const benchmark_event prebuilt{1u};

        {
            const auto thrower = dispatcher.subscribe<benchmark_event>([](const benchmark_event&) { throw 1; });
            try { dispatcher.publish(prebuilt); } catch (...) {}
        }
        for (int i = 0; i < 20'000; ++i)
        {
            const auto transient = dispatcher.subscribe<benchmark_event>([](const benchmark_event&) {});
        }

        catalyst::bench::run("dispatcher.publish (after handler exception + 20k churn)", iterations, [&]
        {
            dispatcher.publish(prebuilt);
        });
    }

    {
        catalyst::core::dispatcher dispatcher;
        const auto subscription = dispatcher.subscribe<benchmark_event>(handle_event);
        catalyst::core::event_queue queue;

        catalyst::bench::run("event_queue.push + dispatch_to (one event per batch)", iterations, [&]
        {
            queue.push<benchmark_event>(1u);
            queue.dispatch_to(dispatcher);
        });
    }

    return handled_events.load(std::memory_order_relaxed) == 0u;
}
