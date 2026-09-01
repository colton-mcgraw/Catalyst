#include <catalyst/input/gamepad.hpp>

#include <catalyst/core/event_sink.hpp>
#include <catalyst/input/input.hpp>

#include "detail_backend.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <optional>
#include <thread>

namespace catalyst::input
{
    namespace
    {
        using clock = std::chrono::steady_clock;

        // Probing an empty XInput slot costs on the order of a millisecond, so empty slots are re-probed at this interval
        // rather than every frame.
        constexpr auto k_empty_slot_probe_interval = std::chrono::milliseconds(1000);

        // How often the blocking calibrator re-reads the controller. XInput refreshes its state every few milliseconds, so
        // reading faster than this only produces duplicate samples.
        constexpr auto k_calibration_read_interval = std::chrono::milliseconds(4);

        struct slot
        {
            gamepad_state state{};
            // The same snapshot before dead zones were applied; exposed by get_gamepad_raw_state() for calibration.
            gamepad_state raw{};
            clock::time_point next_probe{};
        };

        std::array<slot, max_gamepads> g_slots{};
        gamepad_deadzone g_deadzone{};
        const gamepad_state g_disconnected{};

        template <typename E>
        void publish(E e)
        {
            core::event_sink *sink = get_event_sink();
            if (!sink)
                return;
            e.stamp();
            sink->publish(e);
        }

        [[nodiscard]] double clamp_threshold(double t) noexcept
        {
            return std::clamp(t, 0.0, 0.999);
        }

        void apply_deadzones(gamepad_state &s) noexcept
        {
            auto &ax = s.axes;
            apply_radial_deadzone(ax[static_cast<std::size_t>(gamepad_axis::left_x)],
                                  ax[static_cast<std::size_t>(gamepad_axis::left_y)], g_deadzone.stick);
            apply_radial_deadzone(ax[static_cast<std::size_t>(gamepad_axis::right_x)],
                                  ax[static_cast<std::size_t>(gamepad_axis::right_y)], g_deadzone.stick);

            for (const auto a : {gamepad_axis::left_trigger, gamepad_axis::right_trigger})
            {
                double &v = ax[static_cast<std::size_t>(a)];
                v = apply_deadzone(std::clamp(v, 0.0, 1.0), g_deadzone.trigger);
            }
        }

        void publish_differences(gamepad_id id, const gamepad_state &prev, const gamepad_state &cur)
        {
            const gamepad_buttons changed = prev.buttons ^ cur.buttons;
            if (changed != gamepad_buttons::none)
            {
                for (std::size_t i = 0; i < gamepad_button_count; ++i)
                {
                    const auto b = static_cast<gamepad_button>(i);
                    if (!has_button(changed, b))
                        continue;

                    gamepad_button_event e;
                    e.gamepad = id;
                    e.button = b;
                    e.pressed = has_button(cur.buttons, b);
                    publish(e);
                }
            }

            for (std::size_t i = 0; i < gamepad_axis_count; ++i)
            {
                if (prev.axes[i] == cur.axes[i])
                    continue;

                gamepad_axis_event e;
                e.gamepad = id;
                e.axis = static_cast<gamepad_axis>(i);
                e.value = cur.axes[i];
                publish(e);
            }
        }
    } // namespace

    double apply_deadzone(double value, double threshold) noexcept
    {
        const double t = clamp_threshold(threshold);
        const double magnitude = std::fabs(value);
        if (magnitude <= t)
            return 0.0;

        const double scaled = std::min((magnitude - t) / (1.0 - t), 1.0);
        return std::copysign(scaled, value);
    }

    void apply_radial_deadzone(double &x, double &y, double threshold) noexcept
    {
        const double t = clamp_threshold(threshold);
        const double magnitude = std::sqrt(x * x + y * y);
        if (magnitude <= t)
        {
            x = 0.0;
            y = 0.0;
            return;
        }

        const double scaled = std::min((magnitude - t) / (1.0 - t), 1.0);
        const double k = scaled / magnitude;
        x *= k;
        y *= k;
    }

    std::size_t gamepad_capacity() noexcept
    {
        return std::min(detail::gamepad_capacity(), max_gamepads);
    }

    void poll_gamepads()
    {
        const auto now = clock::now();
        const std::size_t capacity = gamepad_capacity();

        for (gamepad_id id = 0; id < capacity; ++id)
        {
            slot &s = g_slots[id];

            if (!s.state.connected && now < s.next_probe)
                continue;

            gamepad_state raw{};
            if (!detail::read_gamepad(id, raw))
            {
                s.next_probe = now + k_empty_slot_probe_interval;

                if (s.state.connected)
                {
                    s.state = gamepad_state{};
                    s.raw = gamepad_state{};

                    gamepad_disconnected_event e;
                    e.gamepad = id;
                    publish(e);
                }
                continue;
            }

            raw.connected = true;
            s.raw = raw;
            apply_deadzones(raw);

            if (!s.state.connected)
            {
                // Diff against an empty state so a controller that is plugged in with buttons held reports them.
                s.state = gamepad_state{};

                gamepad_connected_event e;
                e.gamepad = id;
                publish(e);
            }

            const gamepad_state prev = s.state;
            s.state = raw;
            publish_differences(id, prev, raw);
        }
    }

    const gamepad_state &get_gamepad_state(gamepad_id id) noexcept
    {
        if (id >= max_gamepads)
            return g_disconnected;
        return g_slots[id].state;
    }

    bool is_gamepad_connected(gamepad_id id) noexcept
    {
        return get_gamepad_state(id).connected;
    }

    bool set_gamepad_rumble(gamepad_id id, double low_frequency, double high_frequency) noexcept
    {
        if (id >= gamepad_capacity())
            return false;

        return detail::set_gamepad_rumble(id,
                                          std::clamp(low_frequency, 0.0, 1.0),
                                          std::clamp(high_frequency, 0.0, 1.0));
    }

    void set_gamepad_deadzone(const gamepad_deadzone &dz) noexcept
    {
        g_deadzone.stick = clamp_threshold(dz.stick);
        g_deadzone.trigger = clamp_threshold(dz.trigger);
    }

    gamepad_deadzone get_gamepad_deadzone() noexcept
    {
        return g_deadzone;
    }

    const gamepad_state &get_gamepad_raw_state(gamepad_id id) noexcept
    {
        if (id >= max_gamepads)
            return g_disconnected;
        return g_slots[id].raw;
    }

    // ------------------------------------------------------------------------------------------------------------------
    // Dead-zone calibration
    // ------------------------------------------------------------------------------------------------------------------

    namespace
    {
        [[nodiscard]] double finite_or_zero(double v) noexcept
        {
            return std::isfinite(v) ? v : 0.0;
        }

        [[nodiscard]] double stick_magnitude(const gamepad_state &s, gamepad_axis x, gamepad_axis y) noexcept
        {
            const double vx = finite_or_zero(s.axis(x));
            const double vy = finite_or_zero(s.axis(y));
            return std::sqrt(vx * vx + vy * vy);
        }

        [[nodiscard]] double trigger_magnitude(const gamepad_state &s, gamepad_axis a) noexcept
        {
            return std::fabs(finite_or_zero(s.axis(a)));
        }
    } // namespace

    gamepad_deadzone deadzone_for_noise(double peak_stick, double peak_trigger,
                                        const gamepad_deadzone_calibration_options &opts) noexcept
    {
        const double headroom = std::max(finite_or_zero(opts.headroom), 0.0);
        const double margin = std::max(finite_or_zero(opts.margin), 0.0);

        const auto threshold = [&](double peak) noexcept
        {
            const double p = std::max(finite_or_zero(peak), 0.0);
            return clamp_threshold(p * (1.0 + headroom) + margin);
        };

        gamepad_deadzone dz;
        dz.stick = threshold(peak_stick);
        dz.trigger = threshold(peak_trigger);
        return dz;
    }

    gamepad_deadzone_calibrator::gamepad_deadzone_calibrator(gamepad_id id,
                                                             const gamepad_deadzone_calibration_options &opts) noexcept
        : m_id(id), m_options(opts)
    {
    }

    void gamepad_deadzone_calibrator::start() noexcept
    {
        start(clock::now());
    }

    void gamepad_deadzone_calibrator::start(clock::time_point now) noexcept
    {
        m_status = gamepad_calibration_status::sampling;
        m_restarts = 0;
        restart_window(now);
    }

    void gamepad_deadzone_calibrator::cancel() noexcept
    {
        m_status = gamepad_calibration_status::idle;
    }

    void gamepad_deadzone_calibrator::restart_window(clock::time_point now) noexcept
    {
        m_window_start = now;
        m_last_sample = now;
        m_peak_stick = 0.0;
        m_peak_trigger = 0.0;
    }

    bool gamepad_deadzone_calibrator::update() noexcept
    {
        return sample(get_gamepad_raw_state(m_id), clock::now());
    }

    bool gamepad_deadzone_calibrator::sample(const gamepad_state &raw, clock::time_point now) noexcept
    {
        if (!is_sampling())
            return is_complete();

        if (!raw.connected)
        {
            m_status = gamepad_calibration_status::disconnected;
            return false;
        }

        m_last_sample = now;

        const double stick = std::max(stick_magnitude(raw, gamepad_axis::left_x, gamepad_axis::left_y),
                                      stick_magnitude(raw, gamepad_axis::right_x, gamepad_axis::right_y));
        const double trigger = std::max(trigger_magnitude(raw, gamepad_axis::left_trigger),
                                        trigger_magnitude(raw, gamepad_axis::right_trigger));

        // Anything that looks like deliberate input means the controller is not at rest: throw the window away and wait
        // for the user to let go. A disturbance threshold of 1 or more turns the axis check off.
        const bool axis_check = m_options.disturbance_threshold < 1.0;
        const bool disturbed = raw.buttons != gamepad_buttons::none ||
                               (axis_check && std::max(stick, trigger) > m_options.disturbance_threshold);
        if (disturbed)
        {
            ++m_restarts;
            restart_window(now);
            return false;
        }

        m_peak_stick = std::max(m_peak_stick, stick);
        m_peak_trigger = std::max(m_peak_trigger, trigger);

        if (now - m_window_start >= m_options.duration)
        {
            m_status = gamepad_calibration_status::complete;
            return true;
        }
        return false;
    }

    double gamepad_deadzone_calibrator::progress() const noexcept
    {
        switch (m_status)
        {
        case gamepad_calibration_status::idle:
            return 0.0;
        case gamepad_calibration_status::complete:
            return 1.0;
        case gamepad_calibration_status::sampling:
        case gamepad_calibration_status::disconnected:
            break;
        }

        using seconds = std::chrono::duration<double>;
        const double total = std::chrono::duration_cast<seconds>(m_options.duration).count();
        if (total <= 0.0)
            return 1.0;
        const double elapsed = std::chrono::duration_cast<seconds>(m_last_sample - m_window_start).count();
        return std::clamp(elapsed / total, 0.0, 1.0);
    }

    gamepad_deadzone gamepad_deadzone_calibrator::result() const noexcept
    {
        return deadzone_for_noise(m_peak_stick, m_peak_trigger, m_options);
    }

    bool gamepad_deadzone_calibrator::apply() const noexcept
    {
        if (!is_complete())
            return false;
        set_gamepad_deadzone(result());
        return true;
    }

    std::optional<gamepad_deadzone> calibrate_gamepad_deadzone(gamepad_id id,
                                                               const gamepad_deadzone_calibration_options &opts,
                                                               std::chrono::milliseconds timeout)
    {
        if (id >= gamepad_capacity())
            return std::nullopt;

        gamepad_deadzone_calibrator cal(id, opts);
        const auto begin = clock::now();
        const auto deadline = begin + std::max(timeout, opts.duration);
        cal.start(begin);

        for (;;)
        {
            // Read the hardware directly rather than through poll_gamepads(): this may run before the application has a
            // dispatcher, and it must not publish events or move the snapshots from under a frame loop.
            gamepad_state raw{};
            if (!detail::read_gamepad(id, raw))
                return std::nullopt;
            raw.connected = true;

            const auto now = clock::now();
            if (cal.sample(raw, now))
                return cal.result();
            if (now >= deadline)
                return std::nullopt;

            std::this_thread::sleep_for(k_calibration_read_interval);
        }
    }

} // namespace catalyst::input
