/**
 * @file gamepad.hpp
 * @brief Gamepad support for the Catalyst Input module: button/axis enumerations, the polled gamepad_state snapshot, the
 * events published when that snapshot changes, and the free functions that drive the platform backend.
 * @details Unlike keyboard and mouse input, gamepads are not tied to a window; the input module owns their backend
 * (XInput on Windows, a stub elsewhere). Applications call poll_gamepads() once per frame. Each call reads every slot the
 * backend supports, updates the snapshots returned by get_gamepad_state(), and publishes connect/disconnect, button and
 * axis events describing the differences to the sink installed with input::set_event_sink(). Stick axes are normalised to
 * [-1, 1] with +y up and triggers to [0, 1]; the configurable dead zone is applied before both the snapshot and the events.
 * The dead zone can be learned from a resting controller with gamepad_deadzone_calibrator (frame-driven) or
 * calibrate_gamepad_deadzone() (blocking) instead of being tuned by hand.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

#include <catalyst/core/event.hpp>

namespace catalyst::input
{
    /** @brief Zero-based slot index of a gamepad. Slots are stable while a device stays connected. */
    using gamepad_id = std::uint32_t;

    /** @brief Upper bound on gamepad slots across all backends; gamepad_capacity() reports the backend's actual limit. */
    inline constexpr std::size_t max_gamepads = 8;

    /**
     * @enum gamepad_button
     * @brief Digital gamepad inputs, named positionally after the Xbox layout (a = bottom face button, b = right, x = left,
     * y = top). back/start are the "view"/"menu" buttons on newer controllers; guide is the platform/home button.
     */
    enum class gamepad_button : std::uint8_t
    {
        a,
        b,
        x,
        y,
        back,
        guide,
        start,
        left_stick,
        right_stick,
        left_shoulder,
        right_shoulder,
        dpad_up,
        dpad_down,
        dpad_left,
        dpad_right
    };

    /** @brief Number of values in gamepad_button. */
    inline constexpr std::size_t gamepad_button_count = 15;

    /**
     * @enum gamepad_buttons
     * @brief Bit set of gamepad buttons, one bit per gamepad_button value (bit index == enumerator value).
     */
    enum class gamepad_buttons : std::uint16_t
    {
        none = 0,
        a = 1u << 0,
        b = 1u << 1,
        x = 1u << 2,
        y = 1u << 3,
        back = 1u << 4,
        guide = 1u << 5,
        start = 1u << 6,
        left_stick = 1u << 7,
        right_stick = 1u << 8,
        left_shoulder = 1u << 9,
        right_shoulder = 1u << 10,
        dpad_up = 1u << 11,
        dpad_down = 1u << 12,
        dpad_left = 1u << 13,
        dpad_right = 1u << 14
    };

    [[nodiscard]] inline constexpr gamepad_buttons operator|(gamepad_buttons a, gamepad_buttons b) noexcept
    {
        using u = std::underlying_type_t<gamepad_buttons>;
        return static_cast<gamepad_buttons>(static_cast<u>(a) | static_cast<u>(b));
    }
    [[nodiscard]] inline constexpr gamepad_buttons operator&(gamepad_buttons a, gamepad_buttons b) noexcept
    {
        using u = std::underlying_type_t<gamepad_buttons>;
        return static_cast<gamepad_buttons>(static_cast<u>(a) & static_cast<u>(b));
    }
    [[nodiscard]] inline constexpr gamepad_buttons operator^(gamepad_buttons a, gamepad_buttons b) noexcept
    {
        using u = std::underlying_type_t<gamepad_buttons>;
        return static_cast<gamepad_buttons>(static_cast<u>(a) ^ static_cast<u>(b));
    }
    [[nodiscard]] inline constexpr gamepad_buttons operator~(gamepad_buttons a) noexcept
    {
        using u = std::underlying_type_t<gamepad_buttons>;
        return static_cast<gamepad_buttons>(static_cast<u>(~static_cast<u>(a) & 0x7FFFu));
    }
    inline constexpr gamepad_buttons &operator|=(gamepad_buttons &a, gamepad_buttons b) noexcept { return a = (a | b); }
    inline constexpr gamepad_buttons &operator&=(gamepad_buttons &a, gamepad_buttons b) noexcept { return a = (a & b); }

    /** @brief Converts a single button into its bit-set representation. */
    [[nodiscard]] inline constexpr gamepad_buttons to_gamepad_buttons(gamepad_button b) noexcept
    {
        return static_cast<gamepad_buttons>(1u << static_cast<std::uint8_t>(b));
    }

    /** @brief True if @p set contains @p b. */
    [[nodiscard]] inline constexpr bool has_button(gamepad_buttons set, gamepad_button b) noexcept
    {
        return (set & to_gamepad_buttons(b)) != gamepad_buttons::none;
    }

    /**
     * @enum gamepad_axis
     * @brief Analogue gamepad inputs. Stick axes range over [-1, 1] (+x right, +y up); triggers over [0, 1].
     */
    enum class gamepad_axis : std::uint8_t
    {
        left_x,
        left_y,
        right_x,
        right_y,
        left_trigger,
        right_trigger
    };

    /** @brief Number of values in gamepad_axis. */
    inline constexpr std::size_t gamepad_axis_count = 6;

    /**
     * @struct gamepad_state
     * @brief Snapshot of one gamepad slot as of the last poll_gamepads() call.
     */
    struct gamepad_state
    {
        /** @brief False if nothing is plugged into the slot; the other fields are then zero. */
        bool connected{false};
        /** @brief The buttons currently held. */
        gamepad_buttons buttons{gamepad_buttons::none};
        /** @brief Axis values indexed by gamepad_axis, dead zone already applied. */
        std::array<double, gamepad_axis_count> axes{};

        /** @brief True if @p b is held. */
        [[nodiscard]] constexpr bool is_down(gamepad_button b) const noexcept { return has_button(buttons, b); }
        /** @brief The value of @p a. */
        [[nodiscard]] constexpr double axis(gamepad_axis a) const noexcept { return axes[static_cast<std::size_t>(a)]; }
    };

    /**
     * @struct gamepad_deadzone
     * @brief Dead-zone thresholds applied to raw axis values. Sticks use a radial dead zone (the 2D magnitude is compared
     * against @c stick and the remaining range is rescaled to [0, 1]); triggers use a linear one. The defaults are the
     * values Microsoft recommends for XInput controllers.
     */
    struct gamepad_deadzone
    {
        double stick{7849.0 / 32767.0};
        double trigger{30.0 / 255.0};
    };

    /**
     * @struct gamepad_connected_event
     * @brief A device appeared in a slot (published by poll_gamepads()).
     */
    struct gamepad_connected_event : public core::event<gamepad_connected_event>
    {
        gamepad_id gamepad{0};
    };

    /**
     * @struct gamepad_disconnected_event
     * @brief The device in a slot went away. Releases are *not* published for buttons that were held; consumers that track
     * button state should clear it on this event (input_state does).
     */
    struct gamepad_disconnected_event : public core::event<gamepad_disconnected_event>
    {
        gamepad_id gamepad{0};
    };

    /**
     * @struct gamepad_button_event
     * @brief A button changed state.
     */
    struct gamepad_button_event : public core::event<gamepad_button_event>
    {
        gamepad_id gamepad{0};
        gamepad_button button{gamepad_button::a};
        /** @brief True for a press, false for a release. */
        bool pressed{false};
    };

    /**
     * @struct gamepad_axis_event
     * @brief An axis value changed (after dead-zone processing, so a resting stick produces no events).
     */
    struct gamepad_axis_event : public core::event<gamepad_axis_event>
    {
        gamepad_id gamepad{0};
        gamepad_axis axis{gamepad_axis::left_x};
        /** @brief The new value. */
        double value{0.0};
    };

    /**
     * @fn gamepad_capacity
     * @brief Number of slots the active backend can report (4 for XInput, 0 for the null backend). Always <= max_gamepads.
     */
    [[nodiscard]] std::size_t gamepad_capacity() noexcept;

    /**
     * @fn poll_gamepads
     * @brief Reads every gamepad slot, updates the snapshots and publishes the resulting events to the input event sink.
     * Call once per frame from the thread that owns the sink's dispatcher. Slots that were empty at the previous poll are
     * re-probed only a few times per second, because probing an empty slot is comparatively slow on some backends.
     */
    void poll_gamepads();

    /**
     * @fn get_gamepad_state
     * @brief The snapshot of a slot as of the last poll_gamepads(). Out-of-range ids yield a disconnected state.
     */
    [[nodiscard]] const gamepad_state &get_gamepad_state(gamepad_id id) noexcept;

    /** @brief Shorthand for get_gamepad_state(id).connected. */
    [[nodiscard]] bool is_gamepad_connected(gamepad_id id) noexcept;

    /**
     * @fn set_gamepad_rumble
     * @brief Sets the vibration motors of a gamepad. Intensities are clamped to [0, 1]; pass zeros to stop.
     * @return False if the slot is empty or the backend does not support rumble.
     */
    bool set_gamepad_rumble(gamepad_id id, double low_frequency, double high_frequency) noexcept;

    /** @brief Replaces the dead zone used for subsequent polls. */
    void set_gamepad_deadzone(const gamepad_deadzone &dz) noexcept;

    /** @brief The dead zone currently in use. */
    [[nodiscard]] gamepad_deadzone get_gamepad_deadzone() noexcept;

    /**
     * @fn get_gamepad_raw_state
     * @brief Like get_gamepad_state() but with the axis values exactly as the backend reported them, before any dead
     * zone. @c connected and @c buttons are identical to the processed snapshot. Useful for calibration UIs that want to
     * show a stick's resting noise. Out-of-range ids yield a disconnected state.
     */
    [[nodiscard]] const gamepad_state &get_gamepad_raw_state(gamepad_id id) noexcept;

    // ------------------------------------------------------------------------------------------------------------------
    // Dead-zone calibration
    // ------------------------------------------------------------------------------------------------------------------

    /**
     * @struct gamepad_deadzone_calibration_options
     * @brief Tuning for dead-zone calibration. The controller is watched while it rests for @c duration; the largest
     * radial stick magnitude and the largest trigger value seen in that window are the "noise peaks", and the resulting
     * thresholds are placed just above them: `peak * (1 + headroom) + margin`.
     */
    struct gamepad_deadzone_calibration_options
    {
        /** @brief How long the controller must rest undisturbed before the calibration completes. */
        std::chrono::milliseconds duration{1000};
        /** @brief Relative headroom above the peak noise (0.25 = 25% above the largest value seen). */
        double headroom{0.25};
        /** @brief Absolute margin added on top of the headroom; also the floor for a controller with no noise at all. */
        double margin{0.02};
        /**
         * @brief A raw stick magnitude or trigger value above this, or any button being held, is taken as the user
         * touching the controller: the peaks are discarded and the rest window starts over. Values >= 1 disable the axis
         * part of the check (buttons still restart).
         */
        double disturbance_threshold{0.5};
    };

    /**
     * @enum gamepad_calibration_status
     * @brief Where a gamepad_deadzone_calibrator is in its life cycle.
     */
    enum class gamepad_calibration_status : std::uint8_t
    {
        /** @brief Not started, or cancelled. */
        idle,
        /** @brief Watching the controller; keep feeding samples. */
        sampling,
        /** @brief The rest window elapsed; result() is valid. */
        complete,
        /** @brief The slot emptied (or was never occupied) while sampling. start() again once it reconnects. */
        disconnected
    };

    /**
     * @fn deadzone_for_noise
     * @brief The dead zone calibration derives from a pair of noise peaks: `peak * (1 + headroom) + margin` for each, clamped
     * to the range set_gamepad_deadzone() accepts. Exposed so the policy can be applied to peaks measured some other way.
     */
    [[nodiscard]] gamepad_deadzone deadzone_for_noise(double peak_stick, double peak_trigger,
                                                      const gamepad_deadzone_calibration_options &opts = {}) noexcept;

    /**
     * @class gamepad_deadzone_calibrator
     * @brief Learns a dead zone from a controller that is left alone, one frame at a time, so a game can run its normal loop
     * (and draw a progress bar) while calibrating.
     * @details Typical use:
     * @code
     *   gamepad_deadzone_calibrator cal(0);
     *   cal.start();                       // e.g. when the user picks "calibrate" in a menu
     *   ...
     *   poll_gamepads();                   // every frame, as usual
     *   if (cal.update())                  // returns true once the controller has rested for opts.duration
     *       cal.apply();                   // set_gamepad_deadzone(cal.result())
     *   draw_progress(cal.progress());
     * @endcode
     * Touching a stick, pulling a trigger or pressing a button restarts the rest window (see
     * gamepad_deadzone_calibration_options::disturbance_threshold); restarts() tells a UI to ask the user to let go. If
     * the controller disconnects the status becomes gamepad_calibration_status::disconnected. sample() accepts states from
     * any source, which is how the calibrator can be driven without hardware; update() is sample() fed from the last
     * poll_gamepads() snapshot.
     */
    class gamepad_deadzone_calibrator
    {
    public:
        using clock = std::chrono::steady_clock;

        gamepad_deadzone_calibrator() noexcept = default;
        explicit gamepad_deadzone_calibrator(gamepad_id id, const gamepad_deadzone_calibration_options &opts = {}) noexcept;

        /** @brief Begins (or begins again) sampling at clock::now(). Any previous result is discarded. */
        void start() noexcept;
        /** @brief Begins sampling with an explicit start time, for callers that supply their own timestamps to sample(). */
        void start(clock::time_point now) noexcept;
        /** @brief Stops sampling and returns to gamepad_calibration_status::idle. */
        void cancel() noexcept;

        /**
         * @brief Feeds the raw snapshot from the most recent poll_gamepads() for this calibrator's gamepad. Call once per
         * frame after poll_gamepads() while is_sampling().
         * @return True once the calibration is complete (also on later calls, until start() or cancel()).
         */
        bool update() noexcept;

        /**
         * @brief Feeds one raw (pre-dead-zone) state observed at @p now. Does nothing unless sampling. A disconnected
         * state moves the calibrator to gamepad_calibration_status::disconnected.
         * @return True once the calibration is complete.
         */
        bool sample(const gamepad_state &raw, clock::time_point now) noexcept;

        [[nodiscard]] gamepad_calibration_status status() const noexcept { return m_status; }
        [[nodiscard]] bool is_sampling() const noexcept { return m_status == gamepad_calibration_status::sampling; }
        [[nodiscard]] bool is_complete() const noexcept { return m_status == gamepad_calibration_status::complete; }

        /** @brief Fraction of the current rest window that has elapsed, in [0, 1]; 1 once complete, 0 when idle. */
        [[nodiscard]] double progress() const noexcept;

        /**
         * @brief The dead zone derived from the peaks observed so far (deadzone_for_noise()). Meaningful once complete;
         * before that it reflects a partial window and may still grow.
         */
        [[nodiscard]] gamepad_deadzone result() const noexcept;

        /** @brief Largest radial magnitude seen on either stick in the current window. */
        [[nodiscard]] double peak_stick_noise() const noexcept { return m_peak_stick; }
        /** @brief Largest value seen on either trigger in the current window. */
        [[nodiscard]] double peak_trigger_noise() const noexcept { return m_peak_trigger; }
        /** @brief How many times the rest window was restarted because the controller was disturbed since start(). */
        [[nodiscard]] unsigned restarts() const noexcept { return m_restarts; }

        [[nodiscard]] gamepad_id gamepad() const noexcept { return m_id; }
        [[nodiscard]] const gamepad_deadzone_calibration_options &options() const noexcept { return m_options; }

        /**
         * @brief Installs result() with set_gamepad_deadzone() if the calibration is complete.
         * @return False (and nothing changes) if it is not.
         */
        bool apply() const noexcept;

    private:
        void restart_window(clock::time_point now) noexcept;

        gamepad_id m_id{0};
        gamepad_deadzone_calibration_options m_options{};
        gamepad_calibration_status m_status{gamepad_calibration_status::idle};
        clock::time_point m_window_start{};
        clock::time_point m_last_sample{};
        double m_peak_stick{0.0};
        double m_peak_trigger{0.0};
        unsigned m_restarts{0};
    };

    /**
     * @fn calibrate_gamepad_deadzone
     * @brief Blocking convenience over gamepad_deadzone_calibrator for tools and console programs: reads the controller
     * directly (no events are published and the poll_gamepads() snapshots are untouched) and sleeps between reads until it
     * has rested for @c opts.duration, then returns the learned dead zone. The caller decides whether to install it:
     * @code
     *   if (auto dz = calibrate_gamepad_deadzone(0))
     *       set_gamepad_deadzone(*dz);
     * @endcode
     * @param timeout Gives up after this long (measured from the call and never shorter than @c opts.duration), which
     * happens if the user keeps disturbing the controller.
     * @return Empty if the slot is empty, the controller disconnects, or @p timeout elapses. Do not call this from a
     * frame loop; use gamepad_deadzone_calibrator there.
     */
    [[nodiscard]] std::optional<gamepad_deadzone> calibrate_gamepad_deadzone(
        gamepad_id id, const gamepad_deadzone_calibration_options &opts = {},
        std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    /**
     * @fn apply_deadzone
     * @brief Applies a linear dead zone to a value in [-1, 1]: values inside +/-@p threshold become 0 and the remainder is
     * rescaled so the output still reaches +/-1.
     */
    [[nodiscard]] double apply_deadzone(double value, double threshold) noexcept;

    /**
     * @fn apply_radial_deadzone
     * @brief Applies a radial dead zone to a 2D stick: if the vector's magnitude is below @p threshold both components
     * become 0, otherwise the magnitude is rescaled to [0, 1] and the direction preserved.
     */
    void apply_radial_deadzone(double &x, double &y, double threshold) noexcept;

} // namespace catalyst::input
