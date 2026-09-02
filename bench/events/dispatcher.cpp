#include <benchmark.hpp>

#include <catalyst/core/dispatcher.hpp>
#include <catalyst/core/event.hpp>
#include <catalyst/core/event_queue.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
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

    /**
     * @brief A handler for the concurrent cases that accumulates into thread-local storage.
     * @details handle_event's single shared atomic is a cache line every publishing core has to take exclusively, and with
     * eight threads that one line, not the dispatcher, is what the benchmark would measure. This variant leaves the
     * dispatcher as the only thing shared.
     */
    void handle_event_thread_local(const benchmark_event& event)
    {
        static thread_local std::size_t local_total = 0u;
        local_total += event.value;
        if (local_total == ~std::size_t{0})
            handled_events.fetch_add(1u, std::memory_order_relaxed); // never taken; keeps the accumulation alive
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
    /**
     * @brief Times a block that is run on @p threads threads at once and reports the aggregate rate.
     * @details catalyst::bench::run measures one operation at a time on the calling thread, which cannot express "how
     * many publishes per second does the whole machine manage", so the concurrent cases report themselves. The average is
     * wall time divided by the total operations across all threads, so it is directly comparable with the single-threaded
     * numbers above: equal means one thread's worth of throughput, lower means the design scales.
     */
    template <typename Body>
    void run_parallel(std::string_view name, unsigned threads, std::size_t per_thread, Body&& body)
    {
        using clock = std::chrono::steady_clock;

        const auto start = clock::now();

        std::vector<std::thread> workers;
        workers.reserve(threads);
        for (unsigned t = 0u; t < threads; ++t)
            workers.emplace_back([&] { body(per_thread); });
        for (auto& worker : workers)
            worker.join();

        const auto elapsed = clock::now() - start;
        const auto total = static_cast<double>(per_thread) * threads;
        const auto elapsed_seconds = std::chrono::duration<double>(elapsed).count();

        std::cout << name << '\n'
                  << "  iterations: " << static_cast<std::size_t>(total) << " (" << threads << " threads)\n"
                  << "  total:      " << std::fixed << std::setprecision(3) << (elapsed_seconds * 1'000.0) << " ms\n"
                  << "  average:    " << (std::chrono::duration<double, std::nano>(elapsed).count() / total) << " ns/op\n"
                  << "  throughput: " << (total / elapsed_seconds) << " op/s\n";
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

        // A drain that takes the whole batch in one go is what a frame loop actually does, and it is where the queue's
        // per-event cost should be: one lock acquisition for the batch instead of one per event.
        catalyst::bench::run("event_queue.push + dispatch_to (256-event batches)", iterations / 256u, [&]
        {
            for (std::size_t i = 0u; i < 256u; ++i)
                queue.push<benchmark_event>(1u);
            queue.dispatch_to(dispatcher);
        });

        // Coalescing turns a burst of same-slot events into one queued event and one delivery.
        catalyst::bench::run("event_queue.push_coalescing (256 same-slot events per batch)", iterations / 256u, [&]
        {
            for (std::size_t i = 0u; i < 256u; ++i)
                queue.push_coalescing<benchmark_event>(1u, 1u);
            queue.dispatch_to(dispatcher);
        });
    }

    {
        // The single-threaded reference point for the concurrent rows below: same handler, one thread.
        catalyst::core::dispatcher dispatcher;
        const auto subscription = dispatcher.subscribe<benchmark_event>(handle_event_thread_local);
        const benchmark_event prebuilt{1u};

        catalyst::bench::run("dispatcher.publish (1 thread, thread-local handler)", iterations, [&]
        {
            dispatcher.publish(prebuilt);
        });
    }

    // Contention: what the copy-on-write handler tables buy. Publishing the same event type from every core has to scale,
    // because publishes share a reader lock for a hash lookup and then run the handlers with no lock held at all.
    for (const unsigned threads : {2u, 4u, 8u})
    {
        catalyst::core::dispatcher dispatcher;
        const auto subscription = dispatcher.subscribe<benchmark_event>(handle_event_thread_local);
        const benchmark_event prebuilt{1u};

        run_parallel("dispatcher.publish (" + std::to_string(threads) + " threads, one subscriber)",
                     threads,
                     iterations / threads,
                     [&](std::size_t n) { for (std::size_t i = 0u; i < n; ++i) dispatcher.publish(prebuilt); });
    }

    // The same thing with a subscription churning on one of the threads: a writer taking the exclusive lock to swap in a
    // new handler table must not stall the publishers for longer than the swap itself.
    {
        catalyst::core::dispatcher dispatcher;
        const auto subscription = dispatcher.subscribe<benchmark_event>(handle_event_thread_local);
        const benchmark_event prebuilt{1u};
        std::atomic<bool> churning{true};

        std::thread churn([&]
        {
            while (churning.load(std::memory_order_acquire))
            {
                const auto transient = dispatcher.subscribe<benchmark_event>([](const benchmark_event&) {});
            }
        });

        run_parallel("dispatcher.publish (4 threads + a subscribe/unsubscribe churn thread)",
                     4u,
                     iterations / 4u,
                     [&](std::size_t n) { for (std::size_t i = 0u; i < n; ++i) dispatcher.publish(prebuilt); });

        churning.store(false, std::memory_order_release);
        churn.join();
    }

    // Many producers feeding one draining consumer, which is the shape of every worker-thread-to-frame-loop handoff.
    {
        catalyst::core::dispatcher dispatcher;
        const auto subscription = dispatcher.subscribe<benchmark_event>(handle_event);
        catalyst::core::event_queue queue;

        std::atomic<bool> producing{true};
        std::thread consumer([&]
        {
            while (producing.load(std::memory_order_acquire) || !queue.empty())
                queue.dispatch_to(dispatcher);
        });

        run_parallel("event_queue.push (4 producer threads, 1 draining consumer)",
                     4u,
                     iterations / 4u,
                     [&](std::size_t n) { for (std::size_t i = 0u; i < n; ++i) queue.push<benchmark_event>(1u); });

        producing.store(false, std::memory_order_release);
        consumer.join();
    }

    return handled_events.load(std::memory_order_relaxed) == 0u;
}
