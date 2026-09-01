#include "../core/test_common.hpp"

#include <catalyst/core/dispatcher.hpp>
#include <catalyst/core/event_sink.hpp>
#include <catalyst/input/gamepad.hpp>
#include <catalyst/input/input.hpp>

#include <chrono>
#include <cmath>

using namespace catalyst;
using namespace catalyst::input;
using namespace std::chrono_literals;

namespace
{
    bool near(double a, double b, double eps = 1e-9)
    {
        return std::fabs(a - b) <= eps;
    }

    // A connected raw (pre-dead-zone) snapshot with the given axis values.
    gamepad_state raw_state(double lx, double ly, double rx = 0.0, double ry = 0.0, double lt = 0.0, double rt = 0.0,
                            gamepad_buttons buttons = gamepad_buttons::none)
    {
        gamepad_state s;
        s.connected = true;
        s.buttons = buttons;
        s.axes[static_cast<std::size_t>(gamepad_axis::left_x)] = lx;
        s.axes[static_cast<std::size_t>(gamepad_axis::left_y)] = ly;
        s.axes[static_cast<std::size_t>(gamepad_axis::right_x)] = rx;
        s.axes[static_cast<std::size_t>(gamepad_axis::right_y)] = ry;
        s.axes[static_cast<std::size_t>(gamepad_axis::left_trigger)] = lt;
        s.axes[static_cast<std::size_t>(gamepad_axis::right_trigger)] = rt;
        return s;
    }

    void test_deadzone_for_noise()
    {
        // Defaults: 25% headroom plus an absolute 0.02 margin above the peak.
        const gamepad_deadzone dz = deadzone_for_noise(0.10, 0.04);
        CT_REQUIRE(near(dz.stick, 0.10 * 1.25 + 0.02));
        CT_REQUIRE(near(dz.trigger, 0.04 * 1.25 + 0.02));

        // A controller with no noise at all still gets the margin as a floor.
        const gamepad_deadzone quiet = deadzone_for_noise(0.0, 0.0);
        CT_REQUIRE(near(quiet.stick, 0.02) && near(quiet.trigger, 0.02));

        gamepad_deadzone_calibration_options exact_opts;
        exact_opts.headroom = 0.0;
        exact_opts.margin = 0.0;
        const gamepad_deadzone exact = deadzone_for_noise(0.3, 0.1, exact_opts);
        CT_REQUIRE(near(exact.stick, 0.3) && near(exact.trigger, 0.1));

        // Results always stay inside the range set_gamepad_deadzone() accepts, whatever the peaks were.
        const gamepad_deadzone huge = deadzone_for_noise(5.0, 5.0);
        CT_REQUIRE(huge.stick < 1.0 && huge.trigger < 1.0);
        const gamepad_deadzone negative = deadzone_for_noise(-1.0, -1.0, exact_opts);
        CT_REQUIRE(negative.stick == 0.0 && negative.trigger == 0.0);
        const gamepad_deadzone nan = deadzone_for_noise(std::nan(""), std::nan(""));
        CT_REQUIRE(std::isfinite(nan.stick) && std::isfinite(nan.trigger));
    }

    void test_calibrator_learns_noise()
    {
        using clock = gamepad_deadzone_calibrator::clock;
        gamepad_deadzone_calibration_options opts;
        opts.duration = 100ms;

        gamepad_deadzone_calibrator cal(0, opts);
        CT_REQUIRE(cal.gamepad() == 0);
        CT_REQUIRE(cal.options().duration == 100ms);
        CT_REQUIRE(cal.status() == gamepad_calibration_status::idle);
        CT_REQUIRE(cal.progress() == 0.0);
        // Until start() the calibrator ignores samples and update() is a harmless no-op.
        CT_REQUIRE(!cal.sample(raw_state(0.5, 0.5), clock::now()));
        CT_REQUIRE(!cal.update());
        CT_REQUIRE(cal.status() == gamepad_calibration_status::idle);
        CT_REQUIRE(cal.peak_stick_noise() == 0.0);

        const clock::time_point t0 = clock::now();
        cal.start(t0);
        CT_REQUIRE(cal.is_sampling());
        CT_REQUIRE(!cal.is_complete());

        // Resting noise: the left stick wobbles with magnitude 0.05, the right reaches 0.06, a trigger reads 0.02.
        CT_REQUIRE(!cal.sample(raw_state(0.03, 0.04), t0));
        CT_REQUIRE(near(cal.peak_stick_noise(), 0.05));
        CT_REQUIRE(!cal.sample(raw_state(-0.02, 0.01, 0.0, -0.06, 0.0, 0.02), t0 + 50ms));
        CT_REQUIRE(near(cal.progress(), 0.5));
        CT_REQUIRE(near(cal.peak_stick_noise(), 0.06));
        CT_REQUIRE(near(cal.peak_trigger_noise(), 0.02));
        CT_REQUIRE(cal.is_sampling());

        CT_REQUIRE(cal.sample(raw_state(0.0, 0.0), t0 + 100ms));
        CT_REQUIRE(cal.is_complete());
        CT_REQUIRE(cal.progress() == 1.0);
        CT_REQUIRE(cal.restarts() == 0);

        // The threshold sits just above the largest value seen.
        const gamepad_deadzone dz = cal.result();
        CT_REQUIRE(dz.stick > cal.peak_stick_noise());
        CT_REQUIRE(dz.trigger > cal.peak_trigger_noise());
        CT_REQUIRE(near(dz.stick, 0.06 * 1.25 + 0.02));
        CT_REQUIRE(near(dz.trigger, 0.02 * 1.25 + 0.02));

        // Once complete, further samples are ignored and update() keeps reporting completion.
        CT_REQUIRE(cal.sample(raw_state(0.9, 0.9, 0.0, 0.0, 1.0, 1.0), t0 + 200ms));
        CT_REQUIRE(near(cal.peak_stick_noise(), 0.06));
        CT_REQUIRE(cal.update());
        CT_REQUIRE(near(cal.result().stick, dz.stick));

        // apply() installs the result.
        const gamepad_deadzone before = get_gamepad_deadzone();
        CT_REQUIRE(cal.apply());
        CT_REQUIRE(near(get_gamepad_deadzone().stick, dz.stick));
        CT_REQUIRE(near(get_gamepad_deadzone().trigger, dz.trigger));
        set_gamepad_deadzone(before);

        // cancel() goes back to idle; there is nothing to apply any more.
        cal.cancel();
        CT_REQUIRE(cal.status() == gamepad_calibration_status::idle);
        CT_REQUIRE(!cal.apply());
        CT_REQUIRE(!cal.update());

        // A zero duration completes on the first sample.
        opts.duration = 0ms;
        gamepad_deadzone_calibrator instant(0, opts);
        instant.start(t0);
        CT_REQUIRE(instant.sample(raw_state(0.01, 0.0), t0));
        CT_REQUIRE(instant.is_complete());
    }

    void test_calibrator_restarts_when_disturbed()
    {
        using clock = gamepad_deadzone_calibrator::clock;
        gamepad_deadzone_calibration_options opts;
        opts.duration = 100ms;

        gamepad_deadzone_calibrator cal(0, opts);
        const clock::time_point t0 = clock::now();
        cal.start(t0);

        CT_REQUIRE(!cal.sample(raw_state(0.05, 0.0), t0));
        CT_REQUIRE(near(cal.peak_stick_noise(), 0.05));

        // A stick deflection above the disturbance threshold discards the peaks and restarts the window.
        CT_REQUIRE(!cal.sample(raw_state(0.8, 0.0), t0 + 40ms));
        CT_REQUIRE(cal.restarts() == 1);
        CT_REQUIRE(cal.peak_stick_noise() == 0.0);
        CT_REQUIRE(cal.progress() == 0.0);
        CT_REQUIRE(cal.is_sampling());

        // So does a trigger pull...
        CT_REQUIRE(!cal.sample(raw_state(0.0, 0.0, 0.0, 0.0, 0.0, 0.7), t0 + 50ms));
        CT_REQUIRE(cal.restarts() == 2);

        // ...and a button press, even with the axes perfectly still.
        CT_REQUIRE(!cal.sample(raw_state(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, gamepad_buttons::a), t0 + 60ms));
        CT_REQUIRE(cal.restarts() == 3);

        // The window now runs from t0 + 60ms, so t0 + 100ms is not enough.
        CT_REQUIRE(!cal.sample(raw_state(0.02, 0.0), t0 + 100ms));
        CT_REQUIRE(near(cal.progress(), 0.4));
        CT_REQUIRE(cal.sample(raw_state(0.0, 0.0), t0 + 160ms));
        CT_REQUIRE(cal.is_complete());
        CT_REQUIRE(cal.restarts() == 3);

        // Only the undisturbed window contributes to the result.
        CT_REQUIRE(near(cal.peak_stick_noise(), 0.02));
        CT_REQUIRE(near(cal.result().stick, 0.02 * 1.25 + 0.02));

        // start() clears the restart counter.
        cal.start(t0);
        CT_REQUIRE(cal.restarts() == 0);
        CT_REQUIRE(cal.is_sampling());

        // A disturbance threshold >= 1 turns the axis check off: a full deflection is recorded as noise (and the result
        // is still clamped to something set_gamepad_deadzone() accepts).
        opts.disturbance_threshold = 1.0;
        gamepad_deadzone_calibrator lenient(0, opts);
        lenient.start(t0);
        CT_REQUIRE(!lenient.sample(raw_state(1.0, 0.0), t0));
        CT_REQUIRE(lenient.restarts() == 0);
        CT_REQUIRE(near(lenient.peak_stick_noise(), 1.0));
        CT_REQUIRE(lenient.sample(raw_state(0.0, 0.0), t0 + 100ms));
        CT_REQUIRE(lenient.result().stick < 1.0);
        // Buttons still count.
        lenient.start(t0);
        CT_REQUIRE(!lenient.sample(raw_state(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, gamepad_buttons::start), t0));
        CT_REQUIRE(lenient.restarts() == 1);
    }

    void test_calibrator_handles_disconnect()
    {
        using clock = gamepad_deadzone_calibrator::clock;
        gamepad_deadzone_calibration_options opts;
        opts.duration = 100ms;

        gamepad_deadzone_calibrator cal(0, opts);
        const clock::time_point t0 = clock::now();
        cal.start(t0);
        CT_REQUIRE(!cal.sample(raw_state(0.01, 0.0), t0));

        gamepad_state gone;
        gone.connected = false;
        CT_REQUIRE(!cal.sample(gone, t0 + 50ms));
        CT_REQUIRE(cal.status() == gamepad_calibration_status::disconnected);
        CT_REQUIRE(!cal.is_sampling());
        CT_REQUIRE(!cal.is_complete());
        CT_REQUIRE(!cal.apply());
        // Further samples are ignored until start().
        CT_REQUIRE(!cal.sample(raw_state(0.0, 0.0), t0 + 200ms));
        CT_REQUIRE(cal.status() == gamepad_calibration_status::disconnected);
        cal.start(t0 + 300ms);
        CT_REQUIRE(cal.is_sampling());

        // update() on a slot no backend can fill lands in the same state.
        gamepad_deadzone_calibrator far(max_gamepads, opts);
        far.start();
        CT_REQUIRE(!far.update());
        CT_REQUIRE(far.status() == gamepad_calibration_status::disconnected);
        CT_REQUIRE(!get_gamepad_raw_state(max_gamepads).connected);

        // The blocking helper refuses an out-of-range slot straight away rather than waiting for the timeout.
        const auto began = clock::now();
        CT_REQUIRE(!calibrate_gamepad_deadzone(max_gamepads, opts, 10s).has_value());
        CT_REQUIRE(!calibrate_gamepad_deadzone(max_gamepads + 100).has_value());
        CT_REQUIRE(clock::now() - began < 5s);
    }

    void test_linear_deadzone()
    {
        CT_REQUIRE(apply_deadzone(0.0, 0.24) == 0.0);
        CT_REQUIRE(apply_deadzone(0.1, 0.24) == 0.0);
        CT_REQUIRE(apply_deadzone(-0.1, 0.24) == 0.0);
        CT_REQUIRE(apply_deadzone(1.0, 0.24) == 1.0);
        CT_REQUIRE(apply_deadzone(-1.0, 0.24) == -1.0);
        CT_REQUIRE(near(apply_deadzone(0.62, 0.24), 0.5));
        CT_REQUIRE(near(apply_deadzone(-0.62, 0.24), -0.5));
        // No dead zone is the identity; a silly threshold is clamped rather than dividing by zero.
        CT_REQUIRE(apply_deadzone(0.3, 0.0) == 0.3);
        CT_REQUIRE(std::isfinite(apply_deadzone(1.0, 1.0)));
        CT_REQUIRE(std::isfinite(apply_deadzone(1.0, 5.0)));
    }

    void test_radial_deadzone()
    {
        double x = 0.1, y = 0.1;
        apply_radial_deadzone(x, y, 0.24);
        CT_REQUIRE(x == 0.0 && y == 0.0);

        x = 1.0;
        y = 0.0;
        apply_radial_deadzone(x, y, 0.24);
        CT_REQUIRE(near(x, 1.0) && y == 0.0);

        x = 0.0;
        y = -1.0;
        apply_radial_deadzone(x, y, 0.24);
        CT_REQUIRE(x == 0.0 && near(y, -1.0));

        x = 0.62;
        y = 0.0;
        apply_radial_deadzone(x, y, 0.24);
        CT_REQUIRE(near(x, 0.5) && y == 0.0);

        // Direction is preserved: a unit vector stays a unit vector with the same direction.
        x = 0.6;
        y = 0.8;
        apply_radial_deadzone(x, y, 0.24);
        CT_REQUIRE(near(x, 0.6) && near(y, 0.8));

        // Diagonal at full deflection never exceeds magnitude 1.
        x = 1.0;
        y = 1.0;
        apply_radial_deadzone(x, y, 0.24);
        CT_REQUIRE(near(std::sqrt(x * x + y * y), 1.0));
    }

    void test_deadzone_settings()
    {
        const gamepad_deadzone defaults = get_gamepad_deadzone();
        CT_REQUIRE(near(defaults.stick, 7849.0 / 32767.0));
        CT_REQUIRE(near(defaults.trigger, 30.0 / 255.0));

        set_gamepad_deadzone({0.5, 2.0});
        const gamepad_deadzone after = get_gamepad_deadzone();
        CT_REQUIRE(after.stick == 0.5);
        CT_REQUIRE(after.trigger < 1.0); // clamped

        set_gamepad_deadzone(defaults);
    }

    void test_button_sets()
    {
        CT_REQUIRE(to_gamepad_buttons(gamepad_button::a) == gamepad_buttons::a);
        CT_REQUIRE(to_gamepad_buttons(gamepad_button::dpad_right) == gamepad_buttons::dpad_right);

        gamepad_buttons set = gamepad_buttons::a | gamepad_buttons::start;
        CT_REQUIRE(has_button(set, gamepad_button::a));
        CT_REQUIRE(has_button(set, gamepad_button::start));
        CT_REQUIRE(!has_button(set, gamepad_button::b));

        set &= ~gamepad_buttons::a;
        CT_REQUIRE(!has_button(set, gamepad_button::a));
        CT_REQUIRE((set ^ gamepad_buttons::start) == gamepad_buttons::none);

        gamepad_state s;
        s.buttons = gamepad_buttons::x;
        s.axes[static_cast<std::size_t>(gamepad_axis::right_trigger)] = 0.25;
        CT_REQUIRE(s.is_down(gamepad_button::x));
        CT_REQUIRE(!s.is_down(gamepad_button::y));
        CT_REQUIRE(s.axis(gamepad_axis::right_trigger) == 0.25);
    }

    void test_backend_queries()
    {
        CT_REQUIRE(gamepad_capacity() <= max_gamepads);
        CT_REQUIRE(!get_gamepad_state(max_gamepads).connected);
        CT_REQUIRE(!get_gamepad_state(max_gamepads + 100).connected);
        CT_REQUIRE(!is_gamepad_connected(max_gamepads));
        CT_REQUIRE(!set_gamepad_rumble(max_gamepads, 1.0, 1.0));
        CT_REQUIRE(module_name() != nullptr);
    }

    void test_polling_is_consistent_with_events()
    {
        // The test cannot assume hardware, so it only checks that polling with and without a sink is safe and that the
        // snapshots agree with what was published.
        CT_REQUIRE(get_event_sink() == nullptr);
        poll_gamepads();

        core::dispatcher d;
        core::event_sink sink(d);
        set_event_sink(&sink);
        CT_REQUIRE(get_event_sink() == &sink);

        int connected_events = 0;
        auto sub = d.subscribe<gamepad_connected_event>([&](const gamepad_connected_event &e)
        {
            ++connected_events;
            CT_REQUIRE(e.gamepad < gamepad_capacity());
            CT_REQUIRE(e.has_timestamp());
        });

        // Slots that were probed by the first poll are not re-probed until the probe interval passes, so a device that
        // was already connected then is reported as connected here without a new event; count both ways.
        poll_gamepads();
        std::size_t connected_now = 0;
        for (gamepad_id id = 0; id < gamepad_capacity(); ++id)
            if (is_gamepad_connected(id))
                ++connected_now;
        CT_REQUIRE(static_cast<std::size_t>(connected_events) <= connected_now);

        for (gamepad_id id = 0; id < gamepad_capacity(); ++id)
        {
            const gamepad_state &s = get_gamepad_state(id);
            for (double v : s.axes)
                CT_REQUIRE(v >= -1.0 && v <= 1.0);
            if (!s.connected)
                CT_REQUIRE(s.buttons == gamepad_buttons::none);

            // The raw snapshot mirrors the processed one apart from the dead zone.
            const gamepad_state &raw = get_gamepad_raw_state(id);
            CT_REQUIRE(raw.connected == s.connected);
            CT_REQUIRE(raw.buttons == s.buttons);
            for (double v : raw.axes)
                CT_REQUIRE(v >= -1.0 && v <= 1.0);
            if (!s.connected)
                for (double v : raw.axes)
                    CT_REQUIRE(v == 0.0);
        }

        set_event_sink(nullptr);
        poll_gamepads();
    }
} // namespace

int main()
{
    test_linear_deadzone();
    test_radial_deadzone();
    test_deadzone_settings();
    test_deadzone_for_noise();
    test_calibrator_learns_noise();
    test_calibrator_restarts_when_disturbed();
    test_calibrator_handles_disconnect();
    test_button_sets();
    test_backend_queries();
    test_polling_is_consistent_with_events();
    return 0;
}
