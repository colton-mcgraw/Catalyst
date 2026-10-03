/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Tests for the input bridge: input-module events dispatched on a bus reach a `ui::interaction`.
 * @details Headless: the events are dispatched by hand, as the input context would after the platform
 * fed it, and what the tests check is what the interaction did with them -- who is hovered, and
 * which UI events came back out on the same bus.
 */

#include <catalyst/events/bus.hpp>
#include <catalyst/ui/input_bridge.hpp>
#include <catalyst/ui/ui.hpp>

#include "../test_common.hpp"

#include <cstdint>
#include <string_view>
#include <utility>

using namespace catalyst;

namespace
{
    /** @brief A 100x100 root with one 50x50 child in its top-left corner, laid out. */
    struct fixture
    {
        events::bus bus;
        ui::tree tree;
        ui::node root = ui::null_node;
        ui::node child = ui::null_node;
        ui::interaction ux{tree, bus};

        fixture()
        {
            root = tree.create();
            tree.mutable_style(root).width = ui::px(100.0f);
            tree.mutable_style(root).height = ui::px(100.0f);
            child = tree.create_child(root);
            tree.mutable_style(child).width = ui::px(50.0f);
            tree.mutable_style(child).height = ui::px(50.0f);
            ui::layout(tree, root, ui::layout_params::for_viewport(ui::extent{100.0f, 100.0f}));
            ux.set_root(root);
        }

        void move_to(std::int32_t x, std::int32_t y)
        {
            input::mouse_move_event e;
            e.position_px = math::vec2<std::int32_t>{x, y};
            bus.dispatch(e);
        }

        void button(input::button_action action, std::int32_t x, std::int32_t y,
                    input::key_modifiers mods = input::key_modifiers::none)
        {
            input::mouse_button_event e;
            e.button = input::mouse_button::left;
            e.action = action;
            e.position_px = math::vec2<std::int32_t>{x, y};
            e.modifiers = mods;
            bus.dispatch(e);
        }
    };

    void test_translations()
    {
        CT_REQUIRE(ui::to_pointer_button(input::mouse_button::left) == ui::pointer_button::left);
        CT_REQUIRE(ui::to_pointer_button(input::mouse_button::right) == ui::pointer_button::right);
        CT_REQUIRE(ui::to_pointer_button(input::mouse_button::middle) == ui::pointer_button::middle);
        CT_REQUIRE(ui::to_pointer_button(input::mouse_button::x1) == ui::pointer_button::extra1);
        CT_REQUIRE(ui::to_pointer_button(input::mouse_button::x2) == ui::pointer_button::extra2);
        CT_REQUIRE(ui::to_pointer_button(input::mouse_button::unknown) == ui::pointer_button::none);

        const ui::modifiers m =
            ui::to_modifiers(input::key_modifiers::shift | input::key_modifiers::alt | input::key_modifiers::caps_lock);
        CT_REQUIRE(m.shift);
        CT_REQUIRE(!m.control);
        CT_REQUIRE(m.alt);
        CT_REQUIRE(!m.super);
    }

    void test_pointer_moves_drive_hover()
    {
        fixture f;
        ui::input_bridge bridge(f.bus, f.ux);
        CT_REQUIRE(bridge.active());

        f.move_to(10, 10);
        CT_REQUIRE(f.ux.hovered() == f.child);
        f.move_to(75, 75);
        CT_REQUIRE(f.ux.hovered() == f.root);
    }

    void test_leave_clears_hover_and_enter_restores_it()
    {
        fixture f;
        ui::input_bridge bridge(f.bus, f.ux);

        f.move_to(10, 10);
        CT_REQUIRE(f.ux.hovered() == f.child);

        f.bus.dispatch(input::mouse_leave_event{});
        CT_REQUIRE(ui::is_null(f.ux.hovered()));

        input::mouse_enter_event enter;
        enter.position_px = math::vec2<std::int32_t>{20, 20};
        f.bus.dispatch(enter);
        CT_REQUIRE(f.ux.hovered() == f.child);
    }

    void test_press_and_release_click()
    {
        fixture f;
        ui::input_bridge bridge(f.bus, f.ux);

        int clicks = 0;
        ui::node target = ui::null_node;
        bool shift = false;
        auto token = f.bus.add_listener<ui::click_event>(
            [&](const ui::click_event &e)
            {
                ++clicks;
                target = e.target;
                shift = e.mods.shift;
            });

        f.move_to(10, 10);
        f.button(input::button_action::press, 10, 10, input::key_modifiers::shift);
        f.button(input::button_action::release, 10, 10, input::key_modifiers::shift);
        CT_REQUIRE(clicks == 1);
        CT_REQUIRE(target == f.child);
        CT_REQUIRE(shift);
        token.remove();
    }

    void test_wheel_keys_and_text_are_forwarded()
    {
        fixture f;
        ui::input_bridge bridge(f.bus, f.ux);

        float wheel_y = 0.0f;
        int key_downs = 0;
        int key_ups = 0;
        bool saw_repeat = false;
        std::uint32_t last_key = 0;
        char32_t typed[2]{};
        int typed_count = 0;
        auto t1 = f.bus.add_listener<ui::wheel_event>([&](const ui::wheel_event &e) { wheel_y = e.delta.y(); });
        auto t2 = f.bus.add_listener<ui::key_down_event>(
            [&](const ui::key_down_event &e)
            {
                ++key_downs;
                last_key = e.key;
                saw_repeat = saw_repeat || e.repeat;
            });
        auto t3 = f.bus.add_listener<ui::key_up_event>([&](const ui::key_up_event &) { ++key_ups; });
        auto t4 = f.bus.add_listener<ui::text_input_event>(
            [&](const ui::text_input_event &e)
            {
                if (typed_count < 2)
                    typed[typed_count] = e.code_point;
                ++typed_count;
            });

        f.move_to(10, 10);
        input::mouse_wheel_event wheel;
        wheel.position_px = math::vec2<std::int32_t>{10, 10};
        wheel.delta = math::vec2<float>{0.0f, -2.0f};
        f.bus.dispatch(wheel);
        CT_REQUIRE(wheel_y == -2.0f);

        // Focus a node so key and text events have somewhere to go.
        f.ux.set_focus(f.child);

        input::key_event key;
        key.code = input::key_code::a;
        key.action = input::button_action::press;
        f.bus.dispatch(key);
        key.action = input::button_action::repeat;
        f.bus.dispatch(key);
        key.action = input::button_action::release;
        f.bus.dispatch(key);
        CT_REQUIRE(key_downs == 2);
        CT_REQUIRE(saw_repeat);
        CT_REQUIRE(key_ups == 1);
        CT_REQUIRE(last_key == static_cast<std::uint32_t>(input::key_code::a));

        f.bus.dispatch(input::text_input_event{std::u32string_view{U"hi"}});
        CT_REQUIRE(typed_count == 2);
        CT_REQUIRE(typed[0] == U'h');
        CT_REQUIRE(typed[1] == U'i');

        t1.remove();
        t2.remove();
        t3.remove();
        t4.remove();
    }

    void test_reset_and_destruction_stop_forwarding()
    {
        fixture f;
        {
            ui::input_bridge bridge(f.bus, f.ux);
            f.move_to(10, 10);
            CT_REQUIRE(f.ux.hovered() == f.child);
            bridge.reset();
            CT_REQUIRE(!bridge.active());
            f.move_to(75, 75);
            CT_REQUIRE(f.ux.hovered() == f.child); // Nothing forwarded the move.
        }
        {
            ui::input_bridge bridge(f.bus, f.ux);
        }
        f.move_to(10, 10);
        f.move_to(75, 75);
        CT_REQUIRE(f.ux.hovered() == f.child); // The destroyed bridge left no listener behind.
    }

    void test_move_keeps_forwarding()
    {
        fixture f;
        ui::input_bridge first(f.bus, f.ux);
        ui::input_bridge second = std::move(first);
        CT_REQUIRE(!first.active());
        CT_REQUIRE(second.active());

        f.move_to(10, 10);
        CT_REQUIRE(f.ux.hovered() == f.child);

        ui::input_bridge third;
        CT_REQUIRE(!third.active());
        third = std::move(second);
        f.move_to(75, 75);
        CT_REQUIRE(f.ux.hovered() == f.root);
    }
} // namespace

int main()
{
    test_translations();
    test_pointer_moves_drive_hover();
    test_leave_clears_hover_and_enter_restores_it();
    test_press_and_release_click();
    test_wheel_keys_and_text_are_forwarded();
    test_reset_and_destruction_stop_forwarding();
    test_move_keeps_forwarding();
    return 0;
}
