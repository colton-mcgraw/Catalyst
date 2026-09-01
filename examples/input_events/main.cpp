/*
 * @file main.cpp
 * @brief Example of handling keyboard, text, mouse and gamepad input with the Catalyst input module.
 * @details Creates a window, routes the platform's events and the gamepad poller into one dispatcher, and shows both ways
 * of consuming input: subscribing to the event types directly (used here to echo events to the console) and polling the
 * input_state tracker once per frame (used for the hot keys). Tab toggles cursor capture, which switches the window to
 * raw mouse motion; H toggles a hidden cursor; Space rumbles the first gamepad; Escape quits.
 * License: CDDL-1.0 (see LICENSE).
 */

#include <catalyst/catalyst.hpp>
#include <catalyst/core/dispatcher.hpp>
#include <catalyst/core/event_sink.hpp>
#include <catalyst/input/input.hpp>
#include <catalyst/platform/window.hpp>

#include <chrono>
#include <cstdio>
#include <string>
#include <string_view>
#include <thread>

namespace
{
    std::string to_utf8(std::u32string_view text)
    {
        std::string out;
        for (char32_t cp : text)
        {
            if (cp < 0x80)
                out += static_cast<char>(cp);
            else if (cp < 0x800)
            {
                out += static_cast<char>(0xC0 | (cp >> 6));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
            else if (cp < 0x10000)
            {
                out += static_cast<char>(0xE0 | (cp >> 12));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
            else
            {
                out += static_cast<char>(0xF0 | (cp >> 18));
                out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
        }
        return out;
    }

    std::string modifiers_to_string(catalyst::input::key_modifiers m)
    {
        using catalyst::input::has_modifier;
        using catalyst::input::key_modifiers;

        std::string s;
        if (has_modifier(m, key_modifiers::shift)) s += "shift ";
        if (has_modifier(m, key_modifiers::control)) s += "ctrl ";
        if (has_modifier(m, key_modifiers::alt)) s += "alt ";
        if (has_modifier(m, key_modifiers::super)) s += "super ";
        if (has_modifier(m, key_modifiers::caps_lock)) s += "caps ";
        if (has_modifier(m, key_modifiers::num_lock)) s += "num ";
        if (!s.empty())
            s.pop_back();
        return s.empty() ? "-" : s;
    }

    const char *button_name(catalyst::input::mouse_button b)
    {
        using catalyst::input::mouse_button;
        switch (b)
        {
        case mouse_button::left: return "left";
        case mouse_button::right: return "right";
        case mouse_button::middle: return "middle";
        case mouse_button::x1: return "x1";
        case mouse_button::x2: return "x2";
        default: return "unknown";
        }
    }
} // namespace

int main()
{
    catalyst::catalyst_version_anchor();

    namespace platform = catalyst::platform;
    namespace input = catalyst::input;
    namespace core = catalyst::core;

    platform::window_desc desc;
    desc.title = "Catalyst - input_events";
    desc.width_px = catalyst::ui::px(800.0f);
    desc.height_px = catalyst::ui::px(450.0f);
    desc.visible = true;

    platform::window w = platform::create_window(desc);
    if (!w)
    {
        std::fprintf(stderr, "Failed to create window\n");
        return 1;
    }

    // One dispatcher receives both the window's events and the gamepad poller's events.
    core::dispatcher dispatcher;
    core::event_sink sink(dispatcher);
    platform::set_event_sink(&sink);
    input::set_event_sink(&sink);

    // The tracker answers "is it down / was it pressed this frame" questions; the subscriptions below echo the raw events.
    input::input_state state(dispatcher);

    std::printf("Input example (backend: %s, %zu gamepad slots)\n", input::module_name(), input::gamepad_capacity());
    std::printf("  Escape  quit\n  Tab     toggle cursor capture (raw mouse motion)\n  H       toggle hidden cursor\n"
                "  Space   rumble gamepad 0 while held\n"
                "  C       calibrate gamepad 0's dead zone (then leave the controller alone for a second)\n"
                "  Type, click, scroll and move to see events.\n\n");

    // C learns a dead zone from gamepad 0: whatever noise the sticks and triggers report while the controller rests
    // becomes the new threshold, with a little headroom. The calibrator runs one frame at a time inside the normal loop.
    input::gamepad_deadzone_calibrator calibrator(0);

    bool running = true;
    const auto sub_close = sink.subscribe<platform::window_close_requested_event>([&](const platform::window_close_requested_event &)
    {
        running = false;
    });

    const auto sub_focus = sink.subscribe<platform::window_focus_event>([](const platform::window_focus_event &e)
    {
        std::printf("focus   %s\n", e.focused ? "gained" : "lost");
    });

    const auto sub_key = sink.subscribe<input::key_event>([](const input::key_event &e)
    {
        const char *action = e.action == input::key_action::press ? "press  " : e.action == input::key_action::repeat ? "repeat " : "release";
        std::printf("key     %s %-18s scancode=0x%03X mods=%s\n", action, std::string(input::key_name(e.code)).c_str(),
                    e.scancode, modifiers_to_string(e.modifiers).c_str());
    });

    const auto sub_text = sink.subscribe<input::text_input_event>([](const input::text_input_event &e)
    {
        std::printf("text    \"%s\"\n", to_utf8(e.text()).c_str());
    });

    const auto sub_button = sink.subscribe<input::mouse_button_event>([](const input::mouse_button_event &e)
    {
        std::printf("mouse   %s %s%s at (%d, %d) mods=%s\n",
                    e.action == input::mouse_button_action::press ? "press  " : "release",
                    button_name(e.button), e.clicks >= 2 ? " (double)" : "",
                    e.position_px.x, e.position_px.y, modifiers_to_string(e.modifiers).c_str());
    });

    const auto sub_wheel = sink.subscribe<input::mouse_wheel_event>([](const input::mouse_wheel_event &e)
    {
        std::printf("wheel   (%.2f, %.2f) at (%d, %d)\n", e.delta.x, e.delta.y, e.position_px.x, e.position_px.y);
    });

    const auto sub_enter = sink.subscribe<input::mouse_enter_event>([](const input::mouse_enter_event &e)
    {
        std::printf("mouse   enter at (%d, %d)\n", e.position_px.x, e.position_px.y);
    });

    const auto sub_leave = sink.subscribe<input::mouse_leave_event>([](const input::mouse_leave_event &)
    {
        std::printf("mouse   leave\n");
    });

    const auto sub_pad_connected = sink.subscribe<input::gamepad_connected_event>([](const input::gamepad_connected_event &e)
    {
        std::printf("gamepad %u connected\n", e.gamepad);
    });

    const auto sub_pad_disconnected = sink.subscribe<input::gamepad_disconnected_event>([](const input::gamepad_disconnected_event &e)
    {
        std::printf("gamepad %u disconnected\n", e.gamepad);
    });

    const auto sub_pad_button = sink.subscribe<input::gamepad_button_event>([](const input::gamepad_button_event &e)
    {
        std::printf("gamepad %u button %d %s\n", e.gamepad, static_cast<int>(e.button), e.pressed ? "press" : "release");
    });

    using clock = std::chrono::steady_clock;
    auto last_report = clock::now();
    catalyst::math::vec2<std::int32_t> raw_accum{};

    while (running && platform::is_valid(w))
    {
        state.new_frame();
        platform::pump_events();
        input::poll_gamepads();

        if (state.was_key_pressed(input::key_code::escape))
            running = false;

        if (state.was_key_pressed(input::key_code::tab))
        {
            const bool captured = platform::get_cursor_mode(w) == platform::cursor_mode::captured;
            platform::set_cursor_mode(w, captured ? platform::cursor_mode::normal : platform::cursor_mode::captured);
            std::printf("cursor  %s\n", captured ? "normal" : "captured (raw motion is reported once per second)");
        }

        if (state.was_key_pressed(input::key_code::h))
        {
            const bool hidden = platform::get_cursor_mode(w) == platform::cursor_mode::hidden;
            platform::set_cursor_mode(w, hidden ? platform::cursor_mode::normal : platform::cursor_mode::hidden);
            std::printf("cursor  %s\n", hidden ? "normal" : "hidden");
        }

        if (state.was_key_pressed(input::key_code::space))
            (void)input::set_gamepad_rumble(0, 0.5f, 0.5f);
        if (state.was_key_released(input::key_code::space))
            (void)input::set_gamepad_rumble(0, 0.0f, 0.0f);

        if (state.was_key_pressed(input::key_code::c))
        {
            if (!state.is_gamepad_connected(0))
                std::printf("calib   no gamepad in slot 0\n");
            else
            {
                calibrator.start();
                std::printf("calib   leave gamepad 0 alone for %lld ms...\n",
                            static_cast<long long>(calibrator.options().duration.count()));
            }
        }

        if (calibrator.is_sampling())
        {
            const unsigned restarts_before = calibrator.restarts();
            if (calibrator.update())
            {
                const input::gamepad_deadzone old_dz = input::get_gamepad_deadzone();
                const input::gamepad_deadzone new_dz = calibrator.result();
                std::printf("calib   peak noise stick %.3f trigger %.3f -> dead zone stick %.3f trigger %.3f "
                            "(was %.3f / %.3f)\n",
                            calibrator.peak_stick_noise(), calibrator.peak_trigger_noise(), new_dz.stick, new_dz.trigger,
                            old_dz.stick, old_dz.trigger);
                calibrator.apply();
            }
            else if (calibrator.status() == input::gamepad_calibration_status::disconnected)
                std::printf("calib   gamepad 0 disconnected, calibration abandoned\n");
            else if (calibrator.restarts() != restarts_before)
                std::printf("calib   controller touched, starting over\n");
        }

        raw_accum = raw_accum + state.raw_mouse_delta();

        const auto now = clock::now();
        if (now - last_report >= std::chrono::seconds(1))
        {
            last_report = now;

            if (raw_accum.x != 0 || raw_accum.y != 0)
            {
                std::printf("raw     mouse delta over the last second: (%d, %d)\n", raw_accum.x, raw_accum.y);
                raw_accum = {};
            }

            if (state.is_gamepad_connected(0))
            {
                const double lx = state.gamepad_axis_value(0, input::gamepad_axis::left_x);
                const double ly = state.gamepad_axis_value(0, input::gamepad_axis::left_y);
                const double lt = state.gamepad_axis_value(0, input::gamepad_axis::left_trigger);
                const double rt = state.gamepad_axis_value(0, input::gamepad_axis::right_trigger);
                if (lx != 0.0 || ly != 0.0 || lt != 0.0 || rt != 0.0)
                    std::printf("gamepad 0 left stick (%.2f, %.2f) triggers (%.2f, %.2f)\n", lx, ly, lt, rt);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    platform::set_cursor_mode(w, platform::cursor_mode::normal);
    platform::destroy_window(w);
    platform::set_event_sink(nullptr);
    input::set_event_sink(nullptr);
    return 0;
}
