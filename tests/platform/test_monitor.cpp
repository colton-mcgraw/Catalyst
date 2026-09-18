/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Tests for the catalyst::platform monitor API, against the null backend.
 * @details The null backend reports one 1920x1080 primary monitor at 96 DPI. The values are its
 * own, but the shape of the answers is the contract: the count matches the list, the primary is in
 * the list and flagged as such, an unknown id yields an empty descriptor, and every window sits on
 * some monitor.
 */

#include <catalyst/platform/monitor.hpp>
#include <catalyst/platform/window.hpp>

#include "../test_common.hpp"

#include <string_view>

int main()
{
    using namespace catalyst::platform;

    const std::size_t count = get_monitor_count();
    CT_REQUIRE(count == 1);

    const std::vector<monitor_desc> monitors = get_monitor_list();
    CT_REQUIRE(monitors.size() == count);

    const monitor_id primary = primary_monitor();
    CT_REQUIRE(primary != 0);

    const monitor_desc &m = monitors.front();
    CT_REQUIRE(m.id == primary);
    CT_REQUIRE(m.primary);
    CT_REQUIRE(std::string_view{m.name} == "Null Monitor");
    CT_REQUIRE(m.bounds_px.width() == 1920 && m.bounds_px.height() == 1080);
    CT_REQUIRE(!m.bounds_px.is_empty());
    CT_REQUIRE(m.work_area_px == m.bounds_px);
    CT_REQUIRE(m.dpi_x == 96.0f && m.dpi_y == 96.0f);
    CT_REQUIRE(m.refresh_rate_hz == 60);

    // Looking a monitor up by id gives the same descriptor as the list did.
    const monitor_desc by_id = get_monitor(primary);
    CT_REQUIRE(by_id.id == primary);
    CT_REQUIRE(by_id.bounds_px == m.bounds_px);
    CT_REQUIRE(std::string_view{by_id.name} == std::string_view{m.name});

    // An id nobody reported yields an empty descriptor rather than a guess.
    const monitor_desc unknown = get_monitor(0xFFFF'FFFFu);
    CT_REQUIRE(unknown.id == 0);
    CT_REQUIRE(unknown.bounds_px.is_empty());
    CT_REQUIRE(!unknown.primary);

    // Every window is on some monitor, and with one monitor it is that one.
    window_desc desc;
    desc.visible = false;
    window w = create_window(desc);
    CT_REQUIRE(w);
    CT_REQUIRE(monitor_for_window(w) == primary);
    destroy_window(w);

    return 0;
}
