/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The router: sink registration and removal, per-sink filters, sequence numbering, and
 * middleware ordering and short-circuiting.
 */

#include <catalyst/logging/filter.hpp>
#include <catalyst/logging/router.hpp>
#include <catalyst/logging/sinks/callback.hpp>
#include <catalyst/logging/sinks/ring_buffer.hpp>

#include "../test_common.hpp"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace
{

using catalyst::logging::log_event;
using catalyst::logging::log_level;
using catalyst::logging::router;

log_event make_event(log_level level, std::string message, std::string_view category = "test")
{
    log_event event;
    event.level = level;
    event.category = category;
    event.message = std::move(message);
    return event;
}

void test_sinks_receive_events()
{
    router r;
    std::vector<std::string> seen;

    const auto id = r.add_sink(catalyst::logging::callback_sink{[&](const log_event &e) { seen.push_back(e.message); }});
    CT_REQUIRE(r.sink_count() == 1);

    r.log(make_event(log_level::info, "first"));
    r.log(make_event(log_level::error, "second"));

    CT_REQUIRE(seen.size() == 2);
    CT_REQUIRE(seen[0] == "first");
    CT_REQUIRE(seen[1] == "second");

    // Removing a sink stops delivery, and removing it twice reports that it was already gone.
    CT_REQUIRE(r.remove_sink(id));
    CT_REQUIRE(!r.remove_sink(id));
    CT_REQUIRE(r.sink_count() == 0);

    r.log(make_event(log_level::info, "third"));
    CT_REQUIRE(seen.size() == 2);
}

void test_every_sink_sees_every_event()
{
    router r;
    int a = 0;
    int b = 0;

    r.add_sink(catalyst::logging::callback_sink{[&](const log_event &) { ++a; }});
    r.add_sink(catalyst::logging::callback_sink{[&](const log_event &) { ++b; }});

    r.log(make_event(log_level::warn, "fan out"));

    CT_REQUIRE(a == 1);
    CT_REQUIRE(b == 1);

    r.clear_sinks();
    CT_REQUIRE(r.sink_count() == 0);
}

void test_per_sink_filters()
{
    router r;
    std::vector<std::string> errors;
    std::vector<std::string> everything;

    r.add_sink(catalyst::logging::callback_sink{[&](const log_event &e) { errors.push_back(e.message); }},
               catalyst::logging::level_at_least{log_level::error});
    r.add_sink(catalyst::logging::callback_sink{[&](const log_event &e) { everything.push_back(e.message); }});

    r.log(make_event(log_level::debug, "quiet"));
    r.log(make_event(log_level::error, "loud"));

    // The filter is per sink, not per router: the unfiltered sink still saw both.
    CT_REQUIRE(errors.size() == 1);
    CT_REQUIRE(errors[0] == "loud");
    CT_REQUIRE(everything.size() == 2);
}

void test_filters_can_be_replaced_and_cleared()
{
    router r;
    std::vector<std::string> seen;

    const auto id = r.add_sink(catalyst::logging::callback_sink{[&](const log_event &e) { seen.push_back(e.message); }},
                               catalyst::logging::level_at_least{log_level::error});

    r.log(make_event(log_level::info, "blocked"));
    CT_REQUIRE(seen.empty());

    CT_REQUIRE(r.set_sink_filter(id, catalyst::logging::level_at_least{log_level::trace}));
    r.log(make_event(log_level::info, "allowed"));
    CT_REQUIRE(seen.size() == 1);

    CT_REQUIRE(r.clear_sink_filter(id));
    r.log(make_event(log_level::trace, "also allowed"));
    CT_REQUIRE(seen.size() == 2);

    // A filter cannot be set on a sink that is not there.
    CT_REQUIRE(!r.set_sink_filter(9999, catalyst::logging::level_at_least{log_level::info}));
}

void test_category_filters()
{
    router r;
    std::vector<std::string_view> seen;

    r.add_sink(catalyst::logging::callback_sink{[&](const log_event &e) { seen.push_back(e.category); }},
               catalyst::logging::category_under{"physics"});

    r.log(make_event(log_level::info, "a", "physics"));
    r.log(make_event(log_level::info, "b", "physics.solver"));
    r.log(make_event(log_level::info, "c", "physicsx"));
    r.log(make_event(log_level::info, "d", "audio"));

    // "physics" and anything under it, but not a category that merely starts with the letters.
    CT_REQUIRE(seen.size() == 2);
    CT_REQUIRE(seen[0] == "physics");
    CT_REQUIRE(seen[1] == "physics.solver");
}

void test_sequence_numbers_increase()
{
    router r;
    std::vector<std::uint64_t> sequences;

    r.add_sink(catalyst::logging::callback_sink{[&](const log_event &e) { sequences.push_back(e.sequence); }});

    for (int i = 0; i < 5; ++i)
        r.log(make_event(log_level::info, "n"));

    CT_REQUIRE(sequences.size() == 5);
    for (std::size_t i = 1; i < sequences.size(); ++i)
        CT_REQUIRE(sequences[i] > sequences[i - 1]);

    // A filtered-out event still consumes a sequence number: the count is of events the router
    // handled, not of events that reached a particular sink.
    CT_REQUIRE(r.last_sequence() >= sequences.back());
}

void test_middleware_runs_outermost_first()
{
    router r;
    std::vector<int> order;

    r.add_middleware(
        [&](log_event &event, catalyst::logging::next next) {
            order.push_back(1);
            next(event);
            order.push_back(4);
        },
        10);

    r.add_middleware(
        [&](log_event &event, catalyst::logging::next next) {
            order.push_back(2);
            next(event);
            order.push_back(3);
        },
        5);

    CT_REQUIRE(r.middleware_count() == 2);

    r.add_sink(catalyst::logging::callback_sink{[&](const log_event &) { order.push_back(0); }});
    r.log(make_event(log_level::info, "through the chain"));

    // Descending priority: 10 wraps 5 wraps the sinks, and unwinds in reverse.
    CT_REQUIRE((order == std::vector<int>{1, 2, 0, 3, 4}));
}

void test_middleware_can_edit_and_drop()
{
    router r;
    std::vector<std::string> seen;
    r.add_sink(catalyst::logging::callback_sink{[&](const log_event &e) { seen.push_back(e.message); }});

    const auto rewriter = r.add_middleware([](log_event &event, catalyst::logging::next next) {
        event.message += " (seen)";
        next(event);
    });

    r.log(make_event(log_level::info, "edited"));
    CT_REQUIRE(seen.size() == 1);
    CT_REQUIRE(seen[0] == "edited (seen)");

    // Not calling next drops the event: nothing downstream runs.
    r.add_middleware([](log_event &, catalyst::logging::next) {}, 100);
    r.log(make_event(log_level::error, "dropped"));
    CT_REQUIRE(seen.size() == 1);

    r.clear_middleware();
    CT_REQUIRE(r.middleware_count() == 0);
    CT_REQUIRE(!r.remove_middleware(rewriter));

    r.log(make_event(log_level::info, "plain"));
    CT_REQUIRE(seen.size() == 2);
    CT_REQUIRE(seen[1] == "plain");
}

void test_scoped_sink_removes_itself()
{
    router r;
    CT_REQUIRE(r.sink_count() == 0);

    {
        auto scoped = r.emplace_scoped_sink<catalyst::logging::ring_buffer_sink>(8);
        CT_REQUIRE(r.sink_count() == 1);
        r.log(make_event(log_level::info, "inside"));
    }

    CT_REQUIRE(r.sink_count() == 0);
    r.log(make_event(log_level::info, "after"));
}

} // namespace

int main()
{
    const std::pair<const char *, void (*)()> cases[] = {
        {"sinks receive events", test_sinks_receive_events},
        {"every sink sees every event", test_every_sink_sees_every_event},
        {"per-sink filters", test_per_sink_filters},
        {"filters can be replaced and cleared", test_filters_can_be_replaced_and_cleared},
        {"category filters", test_category_filters},
        {"sequence numbers increase", test_sequence_numbers_increase},
        {"middleware runs outermost first", test_middleware_runs_outermost_first},
        {"middleware can edit and drop", test_middleware_can_edit_and_drop},
        {"scoped sink removes itself", test_scoped_sink_removes_itself},
    };

    for (const auto &[name, fn] : cases)
    {
        std::printf("  %s ...\n", name);
        std::fflush(stdout);
        fn();
    }

    std::printf("catalyst.logging.router: ok\n");
    return 0;
}
