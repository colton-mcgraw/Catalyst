/*
 * @file main.cpp
 * @brief Scratch application for experimenting with the Catalyst libraries.
 * @details This example intentionally does nothing. It exists as a ready-made target to drop
 * throwaway code into while trying something out; the focused examples alongside it show how the
 * individual modules are meant to be used.
 * License: MIT (see LICENSE).
 */

#include <catalyst/catalyst.hpp>

using namespace catalyst;
using namespace catalyst::ui::literals;

namespace
{
    /** @brief Names this example in the log's category column. */
    struct sandbox_log
    {
        static constexpr const char *name = "sandbox";
    };
} // namespace

int main()
{
    (void)catalyst::version();

    // One console sink is all it takes for the calls below to reach the terminal, and colour turns
    // itself on when the stream turns out to be one.
    logging::default_logger().add_sink(logging::console_sink{});

    const auto window = platform::create_window(platform::window_desc{
        .title = "Sandbox Window",
        .width_px = 800.0_px,
        .height_px = 600.0_px,
        .visible = true,
        .resizable = true,
    });

    return 0;
}
