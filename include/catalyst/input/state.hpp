/**
 * @file state.hpp
 * @brief Defines input_state, a polled view of the keyboard, mouse and gamepads built from the event stream. It answers
 * "is this key held?" and "was it pressed this frame?" style questions for game loops that prefer polling to callbacks.
 * @details input_state subscribes to the input events on a dispatcher (at a high priority so that consuming handlers
 * cannot hide events from it) and folds them into per-device state. Level state (held keys, cursor position, held
 * gamepad buttons, axis values) persists until the opposite event arrives; edge state (pressed/released this frame,
 * movement and wheel deltas, typed text) accumulates until the application calls new_frame(), which it should do once
 * per frame *before* pumping events. input_state is not internally synchronised: its handlers run on whichever thread
 * publishes the events, so publish (or drain the event queue feeding it) and call its members from a single thread.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catalyst/core/dispatcher.hpp>
#include <catalyst/core/subscription.hpp>
#include <catalyst/input/gamepad.hpp>
#include <catalyst/input/keyboard.hpp>
#include <catalyst/input/mouse.hpp>
#include <catalyst/math/vec.hpp>

namespace catalyst::input
{
    /**
     * @class input_state
     * @brief Tracks the current state of every input device from the events flowing through a dispatcher.
     *
     * Typical use:
     * @code
     * core::dispatcher d; core::event_sink sink(d); platform::set_event_sink(&sink); input::set_event_sink(&sink);
     * input::input_state in(d);
     * while (running) {
     *     in.new_frame();
     *     platform::pump_events();
     *     input::poll_gamepads();
     *     if (in.was_key_pressed(input::key_code::escape)) running = false;
     *     camera.yaw += in.raw_mouse_delta().x * sensitivity;
     * }
     * @endcode
     */
    class input_state
    {
    public:
        /** @brief Priority used for the tracker's subscriptions; higher than any ordinary handler should use. */
        static constexpr int default_priority = 1'000'000;

        input_state() = default;

        /** @brief Constructs and immediately attaches to @p d. */
        explicit input_state(core::dispatcher &d, int priority = default_priority) { attach(d, priority); }

        input_state(const input_state &) = delete;
        input_state &operator=(const input_state &) = delete;
        input_state(input_state &&) = delete;
        input_state &operator=(input_state &&) = delete;

        ~input_state() = default;

        /**
         * @brief Subscribes to every input event type on @p d. Detaches from any previous dispatcher first. The tracker's
         * handlers never consume events, so lower-priority handlers still see everything.
         */
        void attach(core::dispatcher &d, int priority = default_priority);

        /** @brief Drops all subscriptions and resets every device to its default state. */
        void detach() noexcept;

        /** @brief True while attached to a dispatcher. */
        [[nodiscard]] bool attached() const noexcept { return !m_subscriptions.empty(); }

        /**
         * @brief Clears all per-frame (edge) state: pressed/released flags, mouse and wheel deltas, and typed text. Held
         * state is preserved. Call once per frame before pumping events.
         */
        void new_frame() noexcept;

        /** @brief Forgets all held keys and buttons (e.g. when the application pauses). Edge state is left alone. */
        void release_all() noexcept;

        // ---- Keyboard --------------------------------------------------------------------------------------------

        /** @brief True while the physical key is held. */
        [[nodiscard]] bool is_key_down(key_code code) const noexcept { return m_keys_down.test(index(code)); }
        /** @brief True if the key went down since the last new_frame(). Auto-repeats do not count. */
        [[nodiscard]] bool was_key_pressed(key_code code) const noexcept { return m_keys_pressed.test(index(code)); }
        /** @brief True if the key came up since the last new_frame(). */
        [[nodiscard]] bool was_key_released(key_code code) const noexcept { return m_keys_released.test(index(code)); }
        /** @brief True if the key went down or auto-repeated since the last new_frame() (useful for text navigation). */
        [[nodiscard]] bool was_key_repeated(key_code code) const noexcept { return m_keys_repeated.test(index(code)); }
        /** @brief True if at least one key is held. */
        [[nodiscard]] bool any_key_down() const noexcept { return m_keys_down.any(); }
        /** @brief Number of keys currently held. */
        [[nodiscard]] std::size_t keys_down_count() const noexcept { return m_keys_down.count(); }
        /** @brief Modifier state from the most recent keyboard or mouse event. */
        [[nodiscard]] key_modifiers modifiers() const noexcept { return m_modifiers; }
        /** @brief Text typed since the last new_frame(), in UTF-32. */
        [[nodiscard]] std::u32string_view text() const noexcept { return m_text; }

        // ---- Mouse -----------------------------------------------------------------------------------------------

        /** @brief True while the button is held. */
        [[nodiscard]] bool is_mouse_button_down(mouse_button b) const noexcept { return has_button(m_buttons_down, b); }
        /** @brief True if the button went down since the last new_frame(). */
        [[nodiscard]] bool was_mouse_button_pressed(mouse_button b) const noexcept { return has_button(m_buttons_pressed, b); }
        /** @brief True if the button came up since the last new_frame(). */
        [[nodiscard]] bool was_mouse_button_released(mouse_button b) const noexcept { return has_button(m_buttons_released, b); }
        /** @brief True if the button was pressed with clicks >= 2 since the last new_frame(). */
        [[nodiscard]] bool was_mouse_button_double_clicked(mouse_button b) const noexcept { return has_button(m_buttons_double_clicked, b); }
        /** @brief The set of held buttons. */
        [[nodiscard]] mouse_buttons mouse_buttons_down() const noexcept { return m_buttons_down; }
        /** @brief Last reported cursor position, in client pixels of mouse_window(). */
        [[nodiscard]] math::vec2<std::int32_t> mouse_position() const noexcept { return m_mouse_position; }
        /** @brief Sum of mouse_move_event deltas since the last new_frame(). */
        [[nodiscard]] math::vec2<std::int32_t> mouse_delta() const noexcept { return m_mouse_delta; }
        /** @brief Sum of mouse_raw_move_event deltas since the last new_frame() (non-zero only while the cursor is captured). */
        [[nodiscard]] math::vec2<std::int32_t> raw_mouse_delta() const noexcept { return m_raw_mouse_delta; }
        /** @brief Sum of wheel deltas since the last new_frame(), in notches. */
        [[nodiscard]] math::vec2<float> wheel_delta() const noexcept { return m_wheel_delta; }
        /** @brief The window that produced the most recent mouse event, or 0 if there has been none. */
        [[nodiscard]] std::uint64_t mouse_window() const noexcept { return m_mouse_window; }
        /** @brief True if the cursor is inside the client area of mouse_window(). */
        [[nodiscard]] bool mouse_inside() const noexcept { return m_mouse_inside; }

        // ---- Gamepads --------------------------------------------------------------------------------------------

        /** @brief True if a device is connected in the slot. */
        [[nodiscard]] bool is_gamepad_connected(gamepad_id id) const noexcept { return id < max_gamepads && m_gamepads[id].state.connected; }
        /** @brief True while the button is held. */
        [[nodiscard]] bool is_gamepad_button_down(gamepad_id id, gamepad_button b) const noexcept
        {
            return id < max_gamepads && m_gamepads[id].state.is_down(b);
        }
        /** @brief True if the button went down since the last new_frame(). */
        [[nodiscard]] bool was_gamepad_button_pressed(gamepad_id id, gamepad_button b) const noexcept
        {
            return id < max_gamepads && has_button(m_gamepads[id].pressed, b);
        }
        /** @brief True if the button came up since the last new_frame(). */
        [[nodiscard]] bool was_gamepad_button_released(gamepad_id id, gamepad_button b) const noexcept
        {
            return id < max_gamepads && has_button(m_gamepads[id].released, b);
        }
        /** @brief Current axis value (0 for empty slots). */
        [[nodiscard]] double gamepad_axis_value(gamepad_id id, gamepad_axis a) const noexcept
        {
            return id < max_gamepads ? m_gamepads[id].state.axis(a) : 0.0;
        }
        /** @brief The full snapshot for a slot (a disconnected state for out-of-range ids). */
        [[nodiscard]] const gamepad_state &gamepad(gamepad_id id) const noexcept
        {
            return id < max_gamepads ? m_gamepads[id].state : m_no_gamepad;
        }
        /** @brief True if a device connected in the slot since the last new_frame(). */
        [[nodiscard]] bool was_gamepad_connected(gamepad_id id) const noexcept { return id < max_gamepads && m_gamepads[id].connected_this_frame; }
        /** @brief True if the device in the slot disconnected since the last new_frame(). */
        [[nodiscard]] bool was_gamepad_disconnected(gamepad_id id) const noexcept { return id < max_gamepads && m_gamepads[id].disconnected_this_frame; }

        // ---- Direct feeding --------------------------------------------------------------------------------------
        // These are the handlers attach() subscribes; they are public so that tests, replays and custom event pipelines
        // can feed the tracker without a dispatcher.

        void on_key(const key_event &e) noexcept;
        void on_text(const text_input_event &e);
        void on_mouse_move(const mouse_move_event &e) noexcept;
        void on_mouse_button(const mouse_button_event &e) noexcept;
        void on_mouse_wheel(const mouse_wheel_event &e) noexcept;
        void on_mouse_enter(const mouse_enter_event &e) noexcept;
        void on_mouse_leave(const mouse_leave_event &e) noexcept;
        void on_mouse_raw_move(const mouse_raw_move_event &e) noexcept;
        void on_gamepad_connected(const gamepad_connected_event &e) noexcept;
        void on_gamepad_disconnected(const gamepad_disconnected_event &e) noexcept;
        void on_gamepad_button(const gamepad_button_event &e) noexcept;
        void on_gamepad_axis(const gamepad_axis_event &e) noexcept;

    private:
        struct gamepad_slot
        {
            gamepad_state state{};
            gamepad_buttons pressed{gamepad_buttons::none};
            gamepad_buttons released{gamepad_buttons::none};
            bool connected_this_frame{false};
            bool disconnected_this_frame{false};
        };

        [[nodiscard]] static constexpr std::size_t index(key_code code) noexcept
        {
            const auto v = static_cast<std::size_t>(code);
            return v < key_code_count ? v : 0u;
        }

        std::vector<core::subscription> m_subscriptions;

        std::bitset<key_code_count> m_keys_down;
        std::bitset<key_code_count> m_keys_pressed;
        std::bitset<key_code_count> m_keys_released;
        std::bitset<key_code_count> m_keys_repeated;
        key_modifiers m_modifiers{key_modifiers::none};
        std::u32string m_text;

        mouse_buttons m_buttons_down{mouse_buttons::none};
        mouse_buttons m_buttons_pressed{mouse_buttons::none};
        mouse_buttons m_buttons_released{mouse_buttons::none};
        mouse_buttons m_buttons_double_clicked{mouse_buttons::none};
        math::vec2<std::int32_t> m_mouse_position{};
        math::vec2<std::int32_t> m_mouse_delta{};
        math::vec2<std::int32_t> m_raw_mouse_delta{};
        math::vec2<float> m_wheel_delta{};
        std::uint64_t m_mouse_window{0};
        bool m_mouse_inside{false};

        std::array<gamepad_slot, max_gamepads> m_gamepads{};
        gamepad_state m_no_gamepad{};
    };

} // namespace catalyst::input
