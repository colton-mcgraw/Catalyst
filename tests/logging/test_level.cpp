/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Levels: their order, their names, and the round trip through `parse_level`.
 */

#include <catalyst/logging/level.hpp>

#include "../test_common.hpp"

#include <array>
#include <cstdio>
#include <utility>

namespace
{

using catalyst::logging::log_level;

constexpr std::array all_levels{log_level::trace, log_level::debug, log_level::info,
                                log_level::warn,  log_level::error, log_level::fatal,
                                log_level::critical};

void test_levels_are_ordered()
{
    // Every filter in the module compares levels with <, so the order is load-bearing rather than
    // incidental: trace is the most verbose and must compare least.
    for (std::size_t i = 1; i < all_levels.size(); ++i)
        CT_REQUIRE(all_levels[i - 1] < all_levels[i]);

    CT_REQUIRE(log_level::trace < log_level::critical);
    CT_REQUIRE(log_level::error > log_level::info);
}

void test_every_level_has_a_name()
{
    for (const log_level level : all_levels)
        CT_REQUIRE(!catalyst::logging::name(level).empty());

    CT_REQUIRE(catalyst::logging::name(log_level::trace) == "trace");
    CT_REQUIRE(catalyst::logging::name(log_level::critical) == "critical");
}

void test_parse_level_round_trips()
{
    for (const log_level level : all_levels)
    {
        const auto parsed = catalyst::logging::parse_level(catalyst::logging::name(level));
        CT_REQUIRE(parsed.has_value());
        CT_REQUIRE(*parsed == level);
    }
}

void test_parse_level_rejects_nonsense()
{
    CT_REQUIRE(!catalyst::logging::parse_level("").has_value());
    CT_REQUIRE(!catalyst::logging::parse_level("verbose").has_value());
    CT_REQUIRE(!catalyst::logging::parse_level("inf").has_value());
    CT_REQUIRE(!catalyst::logging::parse_level("info ").has_value());
}

void test_compiled_level_is_a_real_level()
{
    // Set from CATALYST_LOG_COMPILED_LEVEL at configure time, so a typo in the build would land
    // here rather than somewhere confusing.
    bool found = false;
    for (const log_level level : all_levels)
        found = found || level == catalyst::logging::compiled_level;
    CT_REQUIRE(found);
}

} // namespace

int main()
{
    const std::pair<const char *, void (*)()> cases[] = {
        {"levels are ordered", test_levels_are_ordered},
        {"every level has a name", test_every_level_has_a_name},
        {"parse_level round trips", test_parse_level_round_trips},
        {"parse_level rejects nonsense", test_parse_level_rejects_nonsense},
        {"compiled level is a real level", test_compiled_level_is_a_real_level},
    };

    for (const auto &[name, fn] : cases)
    {
        std::printf("  %s ...\n", name);
        std::fflush(stdout);
        fn();
    }

    std::printf("catalyst.logging.level: ok\n");
    return 0;
}
