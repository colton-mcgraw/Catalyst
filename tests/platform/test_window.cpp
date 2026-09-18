/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Tests for the catalyst::platform window API, against the null backend.
 * @details See tests/platform/CMakeLists.txt for why only the null backend. What is asserted here
 * is the contract every backend has to keep: creation announces geometry, state changes are
 * published exactly once, limits clamp, an invalid handle is inert, and a destroyed window says so.
 */

#include <catalyst/events/bus.hpp>
#include <catalyst/platform/platform.hpp>
#include <catalyst/platform/window.hpp>
#include <catalyst/ui/measurement.hpp>

#include "../test_common.hpp"

#include <string_view>

namespace
{
    using namespace catalyst::platform;
    namespace events = catalyst::events;
    namespace ui = catalyst::ui;

    void test_rect_px()
    {
        constexpr rect_px r{{10, 20}, {30, 60}};
        static_assert(r.width() == 20);
        static_assert(r.height() == 40);
        static_assert(r.size().x() == 20 && r.size().y() == 40);
        static_assert(!r.is_empty());
        static_assert(rect_px{}.is_empty());
        static_assert(rect_px{{5, 5}, {5, 9}}.is_empty()); // zero width is empty even with height
        static_assert(r == rect_px{{10, 20}, {30, 60}});

        // module_name() reports the backend in this module, so only its presence is asserted.
        CT_REQUIRE(module_name() != nullptr && !std::string_view{module_name()}.empty());
    }

    void test_invalid_handle_is_inert()
    {
        const window none;
        CT_REQUIRE(!none);
        CT_REQUIRE(none.id() == 0);
        CT_REQUIRE(!is_valid(none));
        CT_REQUIRE(client_rect_px(none).is_empty());
        CT_REQUIRE(dpi_scale(none) == 1.0f);
        CT_REQUIRE(display_state(none) == window_display_state::restored);
        CT_REQUIRE(!is_resizable(none));
        CT_REQUIRE(!is_fullscreen(none));

        // None of these may crash or publish anything for a handle that names no window.
        set_title(none, "nothing");
        set_client_size(none, ui::px(1.0f), ui::px(1.0f));
        set_position(none, {1, 1});
        show(none);
        hide(none);
        minimize(none);
        restore(none);
        focus(none);
        window mutable_none;
        destroy_window(mutable_none);
    }
} // namespace

int main()
{
    test_rect_px();
    test_invalid_handle_is_inert();

    events::bus bus;
    set_event_bus(&bus);
    CT_REQUIRE(event_bus() == &bus);
    CT_REQUIRE(input_feed() == nullptr);

    int resized = 0;
    int moved = 0;
    int dpi = 0;
    int destroyed = 0;
    int state_changes = 0;
    int focused = 0;
    window_id destroyed_id = 0;
    window_display_state last_state = window_display_state::restored;
    std::int32_t last_x = 0;
    std::int32_t last_y = 0;
    float last_width = 0.0f;
    float last_height = 0.0f;

    // scoped_tokens so the listeners come off the bus before it goes out of scope.
    const events::scoped_token on_resized = bus.add_listener<window_resized_event>(
        [&](const window_resized_event &e)
        {
            ++resized;
            ui::resolve_context ctx{};
            ctx.dpi_scale = 1.0f;
            ctx.dpi_x = 96.0f;
            ctx.dpi_y = 96.0f;
            last_width = ui::resolve_or(e.width_px, ui::axis::x, ctx, -1.0f);
            last_height = ui::resolve_or(e.height_px, ui::axis::y, ctx, -1.0f);
        });
    const events::scoped_token on_moved = bus.add_listener<window_moved_event>(
        [&](const window_moved_event &e)
        {
            ++moved;
            last_x = e.position_px.x();
            last_y = e.position_px.y();
        });
    const events::scoped_token on_dpi =
        bus.add_listener<window_dpi_changed_event>([&](const window_dpi_changed_event &) { ++dpi; });
    const events::scoped_token on_destroyed = bus.add_listener<window_destroyed_event>(
        [&](const window_destroyed_event &e)
        {
            ++destroyed;
            destroyed_id = e.window;
        });
    const events::scoped_token on_state = bus.add_listener<window_display_state_event>(
        [&](const window_display_state_event &e)
        {
            ++state_changes;
            last_state = e.state;
        });
    const events::scoped_token on_focus = bus.add_listener<window_focus_event>(
        [&](const window_focus_event &e)
        {
            if (e.focused)
                ++focused;
        });

    // --- Creation announces the initial geometry: one resize, one move, one DPI. ---
    window_desc desc;
    desc.title = "catalyst.platform.window";
    desc.width_px = ui::px(640.0f);
    desc.height_px = ui::px(360.0f);
    desc.visible = false;
    window w = create_window(desc);
    CT_REQUIRE(w);
    CT_REQUIRE(is_valid(w));
    const window_id id = w.id();
    CT_REQUIRE(id != 0);

    CT_REQUIRE(resized == 1 && moved == 1 && dpi == 1);
    CT_REQUIRE(last_width == 640.0f && last_height == 360.0f);
    CT_REQUIRE(client_rect_px(w).width() == 640 && client_rect_px(w).height() == 360);
    CT_REQUIRE(dpi_scale(w) == 1.0f);
    CT_REQUIRE(display_state(w) == window_display_state::restored);
    CT_REQUIRE(is_resizable(w));
    CT_REQUIRE(!is_fullscreen(w));

    // A second window gets a distinct id and does not disturb the first.
    window other = create_window(desc);
    CT_REQUIRE(other && other.id() != id);
    CT_REQUIRE(resized == 2 && moved == 2 && dpi == 2);
    CT_REQUIRE(is_valid(w) && is_valid(other));

    // --- Resizing publishes once, with the new size. ---
    set_client_size(w, ui::px(800.0f), ui::px(450.0f));
    CT_REQUIRE(resized == 3);
    CT_REQUIRE(last_width == 800.0f && last_height == 450.0f);
    CT_REQUIRE(client_rect_px(w).width() == 800 && client_rect_px(w).height() == 450);

    // --- Size limits clamp the current size immediately, and later requests. ---
    set_size_limits(w, {100, 100}, {320, 240});
    CT_REQUIRE(resized == 4);
    CT_REQUIRE(client_rect_px(w).width() == 320 && client_rect_px(w).height() == 240);
    set_client_size(w, ui::px(10.0f), ui::px(10.0f));
    CT_REQUIRE(client_rect_px(w).width() == 100 && client_rect_px(w).height() == 100);
    set_size_limits(w, {0, 0}, {0, 0}); // zero means no limit
    set_client_size(w, ui::px(1000.0f), ui::px(20.0f));
    CT_REQUIRE(client_rect_px(w).width() == 1000 && client_rect_px(w).height() == 20);

    // --- Moving publishes once, and not at all for a no-op. ---
    set_position(w, {50, 60});
    CT_REQUIRE(moved == 3 && last_x == 50 && last_y == 60);
    CT_REQUIRE(position_px(w).x() == 50 && position_px(w).y() == 60);
    set_position(w, {50, 60});
    CT_REQUIRE(moved == 3);

    // --- Display state: each transition publishes once; repeating it does not. ---
    minimize(w);
    CT_REQUIRE(state_changes == 1 && last_state == window_display_state::minimized);
    CT_REQUIRE(display_state(w) == window_display_state::minimized);
    minimize(w);
    CT_REQUIRE(state_changes == 1);
    maximize(w);
    CT_REQUIRE(state_changes == 2 && display_state(w) == window_display_state::maximized);
    restore(w);
    CT_REQUIRE(state_changes == 3 && display_state(w) == window_display_state::restored);

    // Focusing restores a minimised window and reports focus.
    minimize(w);
    CT_REQUIRE(state_changes == 4);
    focus(w);
    CT_REQUIRE(state_changes == 5 && display_state(w) == window_display_state::restored);
    CT_REQUIRE(focused == 1);

    // --- Plain state round-trips. ---
    set_resizable(w, false);
    CT_REQUIRE(!is_resizable(w));
    set_resizable(w, true);
    CT_REQUIRE(is_resizable(w));
    set_fullscreen(w, true);
    CT_REQUIRE(is_fullscreen(w));
    set_fullscreen(w, false);
    CT_REQUIRE(!is_fullscreen(w));
    set_cursor_mode(w, cursor_mode::hidden);
    CT_REQUIRE(get_cursor_mode(w) == cursor_mode::hidden);
    set_cursor_mode(w, cursor_mode::normal);
    CT_REQUIRE(get_cursor_mode(w) == cursor_mode::normal);
    set_title(w, "renamed");
    set_opacity(w, 0.5f);
    show(w);
    hide(w);
    request_attention(w);

    // --- The event loop with nothing to deliver. ---
    pump_events();
    CT_REQUIRE(!wait_events(0));
    CT_REQUIRE(!wait_events(1));
    // An infinite wait must return rather than block: nothing can wake a headless backend.
    CT_REQUIRE(!wait_events());

    // --- Destruction publishes exactly once, invalidates the handle, and leaves the other window alone. ---
    destroy_window(w);
    CT_REQUIRE(destroyed == 1 && destroyed_id == id);
    CT_REQUIRE(!w);
    CT_REQUIRE(!is_valid(window{id}));
    CT_REQUIRE(is_valid(other));
    destroy_window(w); // already gone: no second event
    CT_REQUIRE(destroyed == 1);

    // --- With no bus installed, nothing is published and nothing breaks. ---
    set_event_bus(nullptr);
    CT_REQUIRE(event_bus() == nullptr);
    const int resized_before = resized;
    const int destroyed_before = destroyed;
    window silent = create_window(desc);
    CT_REQUIRE(silent);
    set_client_size(silent, ui::px(2.0f), ui::px(2.0f));
    destroy_window(silent);
    destroy_window(other);
    CT_REQUIRE(resized == resized_before && destroyed == destroyed_before);

    return 0;
}
