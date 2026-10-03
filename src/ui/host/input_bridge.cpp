/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The input-to-UI translation and the listeners that apply it.
 */

#include <catalyst/ui/input_bridge.hpp>

namespace catalyst::ui
{

    pointer_button to_pointer_button(input::mouse_button b) noexcept
    {
        switch (b)
        {
        case input::mouse_button::left:
            return pointer_button::left;
        case input::mouse_button::right:
            return pointer_button::right;
        case input::mouse_button::middle:
            return pointer_button::middle;
        case input::mouse_button::x1:
            return pointer_button::extra1;
        case input::mouse_button::x2:
            return pointer_button::extra2;
        default:
            return pointer_button::none;
        }
    }

    modifiers to_modifiers(input::key_modifiers m) noexcept
    {
        using input::has_modifier;
        using input::key_modifiers;
        return modifiers{
            .shift = has_modifier(m, key_modifiers::shift),
            .control = has_modifier(m, key_modifiers::control),
            .alt = has_modifier(m, key_modifiers::alt),
            .super = has_modifier(m, key_modifiers::super),
        };
    }

    namespace
    {
        [[nodiscard]] point to_point(const math::vec2<std::int32_t> &p) noexcept
        {
            return point{static_cast<float>(p.x()), static_cast<float>(p.y())};
        }
    } // namespace

    input_bridge::input_bridge(events::bus &b, interaction &target)
    {
        interaction *ux = &target;

        tokens_[0] = b.add_listener<input::mouse_move_event>(
            [ux](const input::mouse_move_event &e)
            { ux->pointer_moved(to_point(e.position_px), to_modifiers(e.modifiers)); });

        tokens_[1] = b.add_listener<input::mouse_button_event>(
            [ux](const input::mouse_button_event &e)
            {
                if (e.action == input::button_action::press)
                    ux->pointer_pressed(to_pointer_button(e.button), to_point(e.position_px),
                                        to_modifiers(e.modifiers));
                else if (e.action == input::button_action::release)
                    ux->pointer_released(to_pointer_button(e.button), to_point(e.position_px),
                                         to_modifiers(e.modifiers));
            });

        tokens_[2] = b.add_listener<input::mouse_wheel_event>(
            [ux](const input::mouse_wheel_event &e)
            { ux->wheel(point{e.delta.x(), e.delta.y()}, to_point(e.position_px), to_modifiers(e.modifiers)); });

        // The cursor left the client area: nothing under it is hovered any more. Without this the last
        // node it crossed would stay hovered until the cursor came back.
        tokens_[3] =
            b.add_listener<input::mouse_leave_event>([ux](const input::mouse_leave_event &) { ux->pointer_left(); });

        tokens_[4] = b.add_listener<input::key_event>(
            [ux](const input::key_event &e)
            {
                const auto key = static_cast<std::uint32_t>(e.code);
                if (e.action == input::button_action::release)
                    ux->key_released(key, to_modifiers(e.modifiers));
                else
                    ux->key_pressed(key, to_modifiers(e.modifiers), e.action == input::button_action::repeat);
            });

        tokens_[5] = b.add_listener<input::text_input_event>(
            [ux](const input::text_input_event &e)
            {
                for (const char32_t cp : e.text())
                    ux->text_input(cp);
            });

        // The cursor came back into the client area: hover follows from where it entered rather
        // than waiting for the first move.
        tokens_[6] = b.add_listener<input::mouse_enter_event>([ux](const input::mouse_enter_event &e)
                                                              { ux->pointer_moved(to_point(e.position_px)); });
    }

    void input_bridge::reset()
    {
        for (events::scoped_token &t : tokens_)
            t.reset();
    }

} // namespace catalyst::ui
