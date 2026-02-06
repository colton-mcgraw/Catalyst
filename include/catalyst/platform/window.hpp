#pragma once

#include <catalyst/math/rect.hpp>

#include <cstddef>
#include <cstdint>

namespace catalyst::core
{
    class event_sink;
}

namespace catalyst::platform
{

    using window_id = std::uint64_t;

    enum class native_handle_kind : std::uint8_t
    {
        none,
        win32_hwnd,
    };

    struct native_handle
    {
        native_handle_kind kind = native_handle_kind::none;
        void *handle = nullptr;
        void *extra = nullptr;
    };

    struct window_desc
    {
        const char *title = "Catalyst";
        std::int32_t width_px = 1280;
        std::int32_t height_px = 720;
        bool visible = true;
        bool resizable = true;
    };

    enum class event_type : std::uint8_t
    {
        none,

        window_close_requested,
        window_destroyed,
        window_resized,
        window_enter_size_move,
        window_exit_size_move,
        window_dpi_changed,

        key_down,
        key_up,

        mouse_move,
        mouse_button_down,
        mouse_button_up,
        mouse_wheel,
    };

    enum class mouse_button : std::uint8_t
    {
        left,
        right,
        middle,
        x1,
        x2,
    };

    struct event
    {
        event_type type = event_type::none;
        window_id window = 0;

        union
        {
            struct
            {
                std::int32_t width_px;
                std::int32_t height_px;
            } resized;

            struct
            {
                float dpi_scale;
            } dpi;

            struct
            {
                std::uint32_t key; // platform-defined (e.g. Win32 VK_*)
                bool repeat;
            } key;

            struct
            {
                std::int32_t x_px;
                std::int32_t y_px;
            } mouse_move;

            struct
            {
                mouse_button button;
            } mouse_button;

            struct
            {
                float delta;
            } wheel;
        };

        constexpr event() noexcept : resized{0, 0} {}
    };

    class window
    {
    public:
        window() noexcept = default;
        explicit window(window_id id) noexcept : id_(id) {}

        [[nodiscard]] window_id id() const noexcept { return id_; }
        [[nodiscard]] explicit operator bool() const noexcept { return id_ != 0; }

    private:
        window_id id_{};
    };

    [[nodiscard]] window create_window(const window_desc &desc);
    void destroy_window(window &w) noexcept;

    [[nodiscard]] bool is_valid(const window &w) noexcept;

    [[nodiscard]] native_handle get_native_handle(const window &w) noexcept;
    [[nodiscard]] math::rect<std::int32_t> client_rect_px(const window &w) noexcept;
    [[nodiscard]] float dpi_scale(const window &w) noexcept;

    // Pump OS messages (recommended once per frame).
    void pump_events() noexcept;

    // Block until OS events are available (or timeout).
    // Returns true if the caller should call `pump_events()`; false on timeout.
    // Use `0xFFFFFFFFu` for an infinite wait.
    [[nodiscard]] bool wait_events(std::uint32_t timeout_ms = 0xFFFFFFFFu) noexcept;

    // Retrieve queued events (window + input). Returns false if none available.
    [[nodiscard]] bool poll_event(event &out) noexcept;

    // Optional: provide a sink for translated input events (e.g. mouse move/button/wheel).
    // When set, the active platform backend may publish input events into it.
    void set_event_sink(core::event_sink *sink) noexcept;

} // namespace catalyst::platform
