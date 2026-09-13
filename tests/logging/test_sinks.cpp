/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The sinks that can be inspected without a terminal or a filesystem: the ring buffer, the
 * callback sink, and the async sink that hands work to a thread.
 */

#include <catalyst/logging/router.hpp>
#include <catalyst/logging/sinks/async.hpp>
#include <catalyst/logging/sinks/callback.hpp>
#include <catalyst/logging/sinks/ring_buffer.hpp>

#include "../test_common.hpp"

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{

    using catalyst::logging::log_event;
    using catalyst::logging::log_level;

    log_event make_event(std::string message, log_level level = log_level::info)
    {
        log_event event;
        event.level = level;
        event.category = "test";
        event.message = std::move(message);
        return event;
    }

    void test_ring_buffer_keeps_the_most_recent()
    {
        catalyst::logging::ring_buffer_sink ring{4};
        CT_REQUIRE(ring.capacity() == 4);
        CT_REQUIRE(ring.size() == 0);

        for (int i = 0; i < 3; ++i)
            ring.write(make_event(std::to_string(i)));

        CT_REQUIRE(ring.size() == 3);
        CT_REQUIRE(ring.written() == 3);

        auto snapshot = ring.snapshot();
        CT_REQUIRE(snapshot.size() == 3);
        CT_REQUIRE(snapshot.front().message == "0");
        CT_REQUIRE(snapshot.back().message == "2");
    }

    void test_ring_buffer_wraps()
    {
        catalyst::logging::ring_buffer_sink ring{4};

        for (int i = 0; i < 10; ++i)
            ring.write(make_event(std::to_string(i)));

        // Capacity is a cap on what is kept, not on what is counted: the buffer holds the last four,
        // and still knows that ten went past.
        CT_REQUIRE(ring.size() == 4);
        CT_REQUIRE(ring.written() == 10);

        const auto snapshot = ring.snapshot();
        CT_REQUIRE(snapshot.size() == 4);
        CT_REQUIRE(snapshot.front().message == "6");
        CT_REQUIRE(snapshot.back().message == "9");
    }

    void test_ring_buffer_since()
    {
        catalyst::logging::ring_buffer_sink ring{16};

        for (int i = 0; i < 5; ++i)
        {
            auto event = make_event(std::to_string(i));
            event.sequence = static_cast<std::uint64_t>(i);
            ring.write(std::move(event));
        }

        // `since` is how a crash handler asks for what it has not already reported.
        const auto tail = ring.since(2);
        CT_REQUIRE(tail.size() == 2);
        CT_REQUIRE(tail.front().message == "3");
        CT_REQUIRE(tail.back().message == "4");

        CT_REQUIRE(ring.since(99).empty());

        ring.clear();
        CT_REQUIRE(ring.size() == 0);
        CT_REQUIRE(ring.snapshot().empty());
    }

    void test_callback_sink_default_is_harmless()
    {
        // Default-constructed, so it holds no callback. Writing to it must not dereference one.
        catalyst::logging::callback_sink sink;
        sink.write(make_event("no callback installed"));
    }

    void test_async_sink_delivers_everything()
    {
        // The async sink hands events to a thread, so the assertion that matters is that none are lost
        // between the producer and the drain -- and that flushing is what makes them observable.
        auto inner = std::make_shared<catalyst::logging::ring_buffer_sink>(256);

        {
            catalyst::logging::async_sink async{inner};

            for (int i = 0; i < 100; ++i)
                async.write(make_event(std::to_string(i)));

            async.flush();
            CT_REQUIRE(inner->written() == 100);
        }

        // Destroying the async sink must also drain rather than abandon.
        CT_REQUIRE(inner->written() == 100);
    }

    void test_async_sink_from_many_threads()
    {
        auto inner = std::make_shared<catalyst::logging::ring_buffer_sink>(4096);

        {
            catalyst::logging::async_sink async{inner};

            std::vector<std::thread> threads;
            threads.reserve(4);
            for (int t = 0; t < 4; ++t)
            {
                threads.emplace_back(
                    [&async, t]
                    {
                        for (int i = 0; i < 100; ++i)
                            async.write(make_event(std::to_string(t * 100 + i)));
                    });
            }

            for (auto &thread : threads)
                thread.join();

            async.flush();
        }

        // Four producers, 400 events, none dropped and none double-counted. Run this one under
        // -fsanitize=thread if you touch the queue.
        CT_REQUIRE(inner->written() == 400);
    }

    void test_router_through_a_ring_buffer()
    {
        catalyst::logging::router r;
        auto ring = std::make_shared<catalyst::logging::ring_buffer_sink>(8);
        r.add_sink(ring);

        r.log(make_event("one"));
        r.log(make_event("two", log_level::error));

        const auto snapshot = ring->snapshot();
        CT_REQUIRE(snapshot.size() == 2);
        CT_REQUIRE(snapshot[0].message == "one");
        CT_REQUIRE(snapshot[1].level == log_level::error);

        // The router stamps sequence numbers on the way through.
        CT_REQUIRE(snapshot[1].sequence > snapshot[0].sequence);
    }

} // namespace

int main()
{
    const std::pair<const char *, void (*)()> cases[] = {
        {"ring buffer keeps the most recent", test_ring_buffer_keeps_the_most_recent},
        {"ring buffer wraps", test_ring_buffer_wraps},
        {"ring buffer since", test_ring_buffer_since},
        {"callback sink default is harmless", test_callback_sink_default_is_harmless},
        {"async sink delivers everything", test_async_sink_delivers_everything},
        {"async sink from many threads", test_async_sink_from_many_threads},
        {"router through a ring buffer", test_router_through_a_ring_buffer},
    };

    for (const auto &[name, fn] : cases)
    {
        std::printf("  %s ...\n", name);
        std::fflush(stdout);
        fn();
    }

    std::printf("catalyst.logging.sinks: ok\n");
    return 0;
}
