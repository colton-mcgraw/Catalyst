#include "../core/test_common.hpp"

#include <catalyst/core/dispatcher.hpp>
#include <catalyst/input/state.hpp>

#include <string_view>

using namespace catalyst;
using namespace catalyst::input;

namespace
{
    key_event make_key(key_code code, key_action action, key_modifiers mods = key_modifiers::none)
    {
        key_event e;
        e.window = 1;
        e.code = code;
        e.action = action;
        e.modifiers = mods;
        return e;
    }

    mouse_button_event make_button(mouse_button b, mouse_button_action a, std::uint8_t clicks = 1)
    {
        mouse_button_event e;
        e.window = 1;
        e.button = b;
        e.action = a;
        e.clicks = clicks;
        e.position_px = {10, 20};
        return e;
    }

    void test_key_edges()
    {
        core::dispatcher d;
        input_state in(d);
        CT_REQUIRE(in.attached());

        d.publish(make_key(key_code::space, key_action::press, key_modifiers::shift));
        CT_REQUIRE(in.is_key_down(key_code::space));
        CT_REQUIRE(in.was_key_pressed(key_code::space));
        CT_REQUIRE(in.was_key_repeated(key_code::space));
        CT_REQUIRE(!in.was_key_released(key_code::space));
        CT_REQUIRE(in.any_key_down());
        CT_REQUIRE(in.keys_down_count() == 1);
        CT_REQUIRE(has_modifier(in.modifiers(), key_modifiers::shift));

        in.new_frame();
        CT_REQUIRE(in.is_key_down(key_code::space));
        CT_REQUIRE(!in.was_key_pressed(key_code::space));
        CT_REQUIRE(!in.was_key_repeated(key_code::space));

        // Auto-repeat is not a press.
        d.publish(make_key(key_code::space, key_action::repeat));
        CT_REQUIRE(!in.was_key_pressed(key_code::space));
        CT_REQUIRE(in.was_key_repeated(key_code::space));
        CT_REQUIRE(in.is_key_down(key_code::space));

        in.new_frame();
        d.publish(make_key(key_code::space, key_action::release));
        CT_REQUIRE(!in.is_key_down(key_code::space));
        CT_REQUIRE(in.was_key_released(key_code::space));
        CT_REQUIRE(!in.any_key_down());

        // Press and release within one frame: both edges visible, level state up to date.
        in.new_frame();
        d.publish(make_key(key_code::a, key_action::press));
        d.publish(make_key(key_code::a, key_action::release));
        CT_REQUIRE(in.was_key_pressed(key_code::a));
        CT_REQUIRE(in.was_key_released(key_code::a));
        CT_REQUIRE(!in.is_key_down(key_code::a));

        // Unknown keys are ignored (and never alias to bit 0).
        d.publish(make_key(key_code::unknown, key_action::press));
        CT_REQUIRE(!in.is_key_down(key_code::unknown));
        CT_REQUIRE(!in.was_key_pressed(key_code::unknown));
        d.publish(make_key(static_cast<key_code>(0x4000), key_action::press));
        CT_REQUIRE(in.keys_down_count() == 0);
    }

    void test_tracker_sees_consumed_events()
    {
        core::dispatcher d;
        input_state in(d);

        // A consuming handler at default priority runs after the tracker, so it cannot hide events from it.
        auto sub = d.subscribe<key_event>([](const key_event &) { return true; });

        CT_REQUIRE(d.publish(make_key(key_code::w, key_action::press)));
        CT_REQUIRE(in.is_key_down(key_code::w));
        CT_REQUIRE(in.was_key_pressed(key_code::w));
    }

    void test_text()
    {
        core::dispatcher d;
        input_state in(d);

        text_input_event a(U"ab");
        text_input_event b(U'c');
        d.publish(a);
        d.publish(b);
        CT_REQUIRE(in.text() == U"abc");

        in.new_frame();
        CT_REQUIRE(in.text().empty());
    }

    void test_mouse()
    {
        core::dispatcher d;
        input_state in(d);

        mouse_enter_event enter;
        enter.window = 1;
        enter.position_px = {5, 5};
        d.publish(enter);
        CT_REQUIRE(in.mouse_inside());
        CT_REQUIRE(in.mouse_window() == 1);

        mouse_move_event mv;
        mv.window = 1;
        mv.position_px = {15, 25};
        mv.delta_px = {10, 20};
        mv.buttons = mouse_buttons::left;
        d.publish(mv);
        mv.position_px = {20, 20};
        mv.delta_px = {5, -5};
        d.publish(mv);
        CT_REQUIRE(in.mouse_position().x == 20 && in.mouse_position().y == 20);
        CT_REQUIRE(in.mouse_delta().x == 15 && in.mouse_delta().y == 15);

        d.publish(make_button(mouse_button::left, mouse_button_action::press));
        CT_REQUIRE(in.is_mouse_button_down(mouse_button::left));
        CT_REQUIRE(in.was_mouse_button_pressed(mouse_button::left));
        CT_REQUIRE(!in.was_mouse_button_double_clicked(mouse_button::left));
        CT_REQUIRE(in.mouse_buttons_down() == mouse_buttons::left);
        CT_REQUIRE(in.mouse_position().x == 10 && in.mouse_position().y == 20);

        d.publish(make_button(mouse_button::right, mouse_button_action::press, 2));
        CT_REQUIRE(in.was_mouse_button_double_clicked(mouse_button::right));
        CT_REQUIRE(in.mouse_buttons_down() == (mouse_buttons::left | mouse_buttons::right));

        in.new_frame();
        CT_REQUIRE(in.is_mouse_button_down(mouse_button::left));
        CT_REQUIRE(!in.was_mouse_button_pressed(mouse_button::left));
        CT_REQUIRE(!in.was_mouse_button_double_clicked(mouse_button::right));
        CT_REQUIRE(in.mouse_delta().x == 0 && in.mouse_delta().y == 0);

        d.publish(make_button(mouse_button::left, mouse_button_action::release));
        CT_REQUIRE(!in.is_mouse_button_down(mouse_button::left));
        CT_REQUIRE(in.was_mouse_button_released(mouse_button::left));
        CT_REQUIRE(in.is_mouse_button_down(mouse_button::right));

        // Unknown buttons are ignored.
        d.publish(make_button(mouse_button::unknown, mouse_button_action::press));
        CT_REQUIRE(in.mouse_buttons_down() == mouse_buttons::right);

        mouse_wheel_event wh;
        wh.window = 1;
        wh.delta = {0.0f, 1.5f};
        d.publish(wh);
        wh.delta = {0.5f, -0.5f};
        d.publish(wh);
        CT_REQUIRE(in.wheel_delta().x == 0.5f && in.wheel_delta().y == 1.0f);

        mouse_raw_move_event raw;
        raw.window = 1;
        raw.delta = {3, -4};
        d.publish(raw);
        d.publish(raw);
        CT_REQUIRE(in.raw_mouse_delta().x == 6 && in.raw_mouse_delta().y == -8);

        // A leave for a different window does not change the inside flag.
        mouse_leave_event other_leave;
        other_leave.window = 2;
        d.publish(other_leave);
        CT_REQUIRE(in.mouse_inside());

        mouse_leave_event leave;
        leave.window = 1;
        d.publish(leave);
        CT_REQUIRE(!in.mouse_inside());

        in.new_frame();
        CT_REQUIRE(in.wheel_delta().x == 0.0f && in.wheel_delta().y == 0.0f);
        CT_REQUIRE(in.raw_mouse_delta().x == 0 && in.raw_mouse_delta().y == 0);
    }

    void test_gamepads()
    {
        core::dispatcher d;
        input_state in(d);

        CT_REQUIRE(!in.is_gamepad_connected(0));
        CT_REQUIRE(!in.is_gamepad_connected(max_gamepads + 3)); // out of range is simply "not connected"
        CT_REQUIRE(!in.gamepad(max_gamepads + 3).connected);

        gamepad_connected_event c;
        c.gamepad = 0;
        d.publish(c);
        CT_REQUIRE(in.is_gamepad_connected(0));
        CT_REQUIRE(in.was_gamepad_connected(0));

        gamepad_button_event b;
        b.gamepad = 0;
        b.button = gamepad_button::a;
        b.pressed = true;
        d.publish(b);
        CT_REQUIRE(in.is_gamepad_button_down(0, gamepad_button::a));
        CT_REQUIRE(in.was_gamepad_button_pressed(0, gamepad_button::a));
        CT_REQUIRE(!in.is_gamepad_button_down(0, gamepad_button::b));

        gamepad_axis_event ax;
        ax.gamepad = 0;
        ax.axis = gamepad_axis::left_y;
        ax.value = -0.75f;
        d.publish(ax);
        CT_REQUIRE(in.gamepad_axis_value(0, gamepad_axis::left_y) == -0.75f);
        CT_REQUIRE(in.gamepad(0).axis(gamepad_axis::left_y) == -0.75f);

        in.new_frame();
        CT_REQUIRE(!in.was_gamepad_connected(0));
        CT_REQUIRE(!in.was_gamepad_button_pressed(0, gamepad_button::a));
        CT_REQUIRE(in.is_gamepad_button_down(0, gamepad_button::a));

        b.pressed = false;
        d.publish(b);
        CT_REQUIRE(!in.is_gamepad_button_down(0, gamepad_button::a));
        CT_REQUIRE(in.was_gamepad_button_released(0, gamepad_button::a));

        b.pressed = true;
        d.publish(b);
        gamepad_disconnected_event dc;
        dc.gamepad = 0;
        d.publish(dc);
        CT_REQUIRE(!in.is_gamepad_connected(0));
        CT_REQUIRE(in.was_gamepad_disconnected(0));
        CT_REQUIRE(!in.is_gamepad_button_down(0, gamepad_button::a)); // held buttons are cleared on disconnect
        CT_REQUIRE(in.gamepad_axis_value(0, gamepad_axis::left_y) == 0.0f);

        // Events for slots beyond max_gamepads are ignored rather than indexing out of bounds.
        gamepad_button_event far_button;
        far_button.gamepad = max_gamepads + 1;
        far_button.pressed = true;
        d.publish(far_button);
        gamepad_axis_event far_axis;
        far_axis.gamepad = max_gamepads + 1;
        d.publish(far_axis);
    }

    void test_detach_and_reattach()
    {
        core::dispatcher d;
        input_state in;
        CT_REQUIRE(!in.attached());

        in.attach(d);
        d.publish(make_key(key_code::q, key_action::press));
        CT_REQUIRE(in.is_key_down(key_code::q));

        in.detach();
        CT_REQUIRE(!in.attached());
        CT_REQUIRE(!in.is_key_down(key_code::q)); // detach resets level state
        CT_REQUIRE(d.handler_count<key_event>() == 0);

        d.publish(make_key(key_code::q, key_action::press));
        CT_REQUIRE(!in.is_key_down(key_code::q));

        in.attach(d);
        d.publish(make_key(key_code::q, key_action::press));
        CT_REQUIRE(in.is_key_down(key_code::q));

        // Attaching again does not double-subscribe.
        in.attach(d);
        CT_REQUIRE(d.handler_count<key_event>() == 1);

        in.release_all();
        CT_REQUIRE(!in.is_key_down(key_code::q));
    }

    void test_direct_feed_without_dispatcher()
    {
        input_state in;
        in.on_key(make_key(key_code::escape, key_action::press));
        CT_REQUIRE(in.is_key_down(key_code::escape));
        in.on_mouse_button(make_button(mouse_button::middle, mouse_button_action::press));
        CT_REQUIRE(in.is_mouse_button_down(mouse_button::middle));
    }

    void test_subscriptions_outlive_dispatcher()
    {
        input_state in;
        {
            core::dispatcher d;
            in.attach(d);
            d.publish(make_key(key_code::a, key_action::press));
            CT_REQUIRE(in.is_key_down(key_code::a));
        }
        // The dispatcher is gone; destroying / detaching the tracker must be a no-op rather than a crash.
        in.detach();
        CT_REQUIRE(!in.attached());
    }
} // namespace

int main()
{
    test_key_edges();
    test_tracker_sees_consumed_events();
    test_text();
    test_mouse();
    test_gamepads();
    test_detach_and_reattach();
    test_direct_feed_without_dispatcher();
    test_subscriptions_outlive_dispatcher();
    return 0;
}
