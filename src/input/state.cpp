#include <catalyst/input/state.hpp>

namespace catalyst::input
{
    namespace
    {
        // Text typed in one frame is bounded so a loop that never calls new_frame() cannot grow the buffer forever.
        constexpr std::size_t k_max_text_per_frame = 4096;
    }

    void input_state::attach(core::dispatcher &d, int priority)
    {
        detach();

        m_subscriptions.reserve(12);
        m_subscriptions.push_back(d.subscribe<key_event>([this](const key_event &e) { on_key(e); }, priority));
        m_subscriptions.push_back(d.subscribe<text_input_event>([this](const text_input_event &e) { on_text(e); }, priority));
        m_subscriptions.push_back(d.subscribe<mouse_move_event>([this](const mouse_move_event &e) { on_mouse_move(e); }, priority));
        m_subscriptions.push_back(d.subscribe<mouse_button_event>([this](const mouse_button_event &e) { on_mouse_button(e); }, priority));
        m_subscriptions.push_back(d.subscribe<mouse_wheel_event>([this](const mouse_wheel_event &e) { on_mouse_wheel(e); }, priority));
        m_subscriptions.push_back(d.subscribe<mouse_enter_event>([this](const mouse_enter_event &e) { on_mouse_enter(e); }, priority));
        m_subscriptions.push_back(d.subscribe<mouse_leave_event>([this](const mouse_leave_event &e) { on_mouse_leave(e); }, priority));
        m_subscriptions.push_back(d.subscribe<mouse_raw_move_event>([this](const mouse_raw_move_event &e) { on_mouse_raw_move(e); }, priority));
        m_subscriptions.push_back(d.subscribe<gamepad_connected_event>([this](const gamepad_connected_event &e) { on_gamepad_connected(e); }, priority));
        m_subscriptions.push_back(d.subscribe<gamepad_disconnected_event>([this](const gamepad_disconnected_event &e) { on_gamepad_disconnected(e); }, priority));
        m_subscriptions.push_back(d.subscribe<gamepad_button_event>([this](const gamepad_button_event &e) { on_gamepad_button(e); }, priority));
        m_subscriptions.push_back(d.subscribe<gamepad_axis_event>([this](const gamepad_axis_event &e) { on_gamepad_axis(e); }, priority));
    }

    void input_state::detach() noexcept
    {
        m_subscriptions.clear();

        m_keys_down.reset();
        m_modifiers = key_modifiers::none;
        m_buttons_down = mouse_buttons::none;
        m_mouse_position = {};
        m_mouse_window = 0;
        m_mouse_inside = false;
        for (gamepad_slot &g : m_gamepads)
            g = gamepad_slot{};

        new_frame();
    }

    void input_state::new_frame() noexcept
    {
        m_keys_pressed.reset();
        m_keys_released.reset();
        m_keys_repeated.reset();
        m_text.clear();

        m_buttons_pressed = mouse_buttons::none;
        m_buttons_released = mouse_buttons::none;
        m_buttons_double_clicked = mouse_buttons::none;
        m_mouse_delta = {};
        m_raw_mouse_delta = {};
        m_wheel_delta = {};

        for (gamepad_slot &g : m_gamepads)
        {
            g.pressed = gamepad_buttons::none;
            g.released = gamepad_buttons::none;
            g.connected_this_frame = false;
            g.disconnected_this_frame = false;
        }
    }

    void input_state::release_all() noexcept
    {
        m_keys_down.reset();
        m_buttons_down = mouse_buttons::none;
        for (gamepad_slot &g : m_gamepads)
            g.state.buttons = gamepad_buttons::none;
    }

    // ---- Keyboard ------------------------------------------------------------------------------------------------

    void input_state::on_key(const key_event &e) noexcept
    {
        m_modifiers = e.modifiers;

        if (e.code == key_code::unknown || static_cast<std::size_t>(e.code) >= key_code_count)
            return;

        const std::size_t i = index(e.code);
        switch (e.action)
        {
        case key_action::press:
            m_keys_pressed.set(i);
            m_keys_repeated.set(i);
            m_keys_down.set(i);
            break;
        case key_action::repeat:
            m_keys_repeated.set(i);
            m_keys_down.set(i);
            break;
        case key_action::release:
            m_keys_released.set(i);
            m_keys_down.reset(i);
            break;
        }
    }

    void input_state::on_text(const text_input_event &e)
    {
        m_modifiers = e.modifiers;

        const std::u32string_view text = e.text();
        if (m_text.size() >= k_max_text_per_frame)
            return;
        m_text.append(text.substr(0, k_max_text_per_frame - m_text.size()));
    }

    // ---- Mouse ---------------------------------------------------------------------------------------------------

    void input_state::on_mouse_move(const mouse_move_event &e) noexcept
    {
        m_modifiers = e.modifiers;
        m_mouse_window = e.window;
        m_mouse_position = e.position_px;
        m_mouse_delta = m_mouse_delta + e.delta_px;
    }

    void input_state::on_mouse_button(const mouse_button_event &e) noexcept
    {
        m_modifiers = e.modifiers;
        m_mouse_window = e.window;
        m_mouse_position = e.position_px;

        const mouse_buttons bit = to_mouse_buttons(e.button);
        if (bit == mouse_buttons::none)
            return;

        if (e.action == mouse_button_action::press)
        {
            m_buttons_down |= bit;
            m_buttons_pressed |= bit;
            if (e.clicks >= 2)
                m_buttons_double_clicked |= bit;
        }
        else
        {
            m_buttons_down &= ~bit;
            m_buttons_released |= bit;
        }
    }

    void input_state::on_mouse_wheel(const mouse_wheel_event &e) noexcept
    {
        m_modifiers = e.modifiers;
        m_mouse_window = e.window;
        m_mouse_position = e.position_px;
        m_wheel_delta = m_wheel_delta + e.delta;
    }

    void input_state::on_mouse_enter(const mouse_enter_event &e) noexcept
    {
        m_mouse_window = e.window;
        m_mouse_position = e.position_px;
        m_mouse_inside = true;
    }

    void input_state::on_mouse_leave(const mouse_leave_event &e) noexcept
    {
        if (e.window == m_mouse_window)
            m_mouse_inside = false;
    }

    void input_state::on_mouse_raw_move(const mouse_raw_move_event &e) noexcept
    {
        m_raw_mouse_delta = m_raw_mouse_delta + e.delta;
    }

    // ---- Gamepads ------------------------------------------------------------------------------------------------

    void input_state::on_gamepad_connected(const gamepad_connected_event &e) noexcept
    {
        if (e.gamepad >= max_gamepads)
            return;

        gamepad_slot &g = m_gamepads[e.gamepad];
        g.state = gamepad_state{};
        g.state.connected = true;
        g.connected_this_frame = true;
    }

    void input_state::on_gamepad_disconnected(const gamepad_disconnected_event &e) noexcept
    {
        if (e.gamepad >= max_gamepads)
            return;

        gamepad_slot &g = m_gamepads[e.gamepad];
        g.state = gamepad_state{};
        g.disconnected_this_frame = true;
    }

    void input_state::on_gamepad_button(const gamepad_button_event &e) noexcept
    {
        if (e.gamepad >= max_gamepads)
            return;

        gamepad_slot &g = m_gamepads[e.gamepad];
        const gamepad_buttons bit = to_gamepad_buttons(e.button);
        g.state.connected = true;
        if (e.pressed)
        {
            g.state.buttons |= bit;
            g.pressed |= bit;
        }
        else
        {
            g.state.buttons &= ~bit;
            g.released |= bit;
        }
    }

    void input_state::on_gamepad_axis(const gamepad_axis_event &e) noexcept
    {
        if (e.gamepad >= max_gamepads || static_cast<std::size_t>(e.axis) >= gamepad_axis_count)
            return;

        gamepad_slot &g = m_gamepads[e.gamepad];
        g.state.connected = true;
        g.state.axes[static_cast<std::size_t>(e.axis)] = e.value;
    }

} // namespace catalyst::input
