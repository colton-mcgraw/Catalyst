/*
 * @file main.cpp
 * @brief Example of using the Catalyst platform library to create a window and poll for events.
 * @details This example demonstrates how to initialize the Catalyst platform library, create a window, and enter a main loop that polls for events. The example handles window close requests and resize events, printing relevant information to the console. It simulates a simple frame loop with a sleep to mimic a 60 Hz update rate. This serves as a basic template for using the Catalyst platform library in applications that require window management and event handling.
 * License: CDDL-1.0 (see LICENSE).
 */

#include <catalyst/catalyst.hpp>
#include <catalyst/core/dispatcher.hpp>
#include <catalyst/core/event_sink.hpp>
#include <catalyst/platform/window.hpp>

#include <cstdio>
#include <chrono>
#include <thread>

int main()
{
  catalyst::catalyst_version_anchor();

  using namespace catalyst::platform;

  window_desc desc;
  desc.title = "Catalyst - window_polling";
  desc.width_px = catalyst::ui::px(800.0f);
  desc.height_px = catalyst::ui::px(450.0f);
  desc.visible = true;

  window w = create_window(desc);
  if (!w)
  {
    std::fprintf(stderr, "Failed to create window\n");
    return 1;
  }

  std::printf("Polling example: call pump_events() once per frame.\n");

  catalyst::core::dispatcher dispatcher;
  catalyst::core::event_sink sink(dispatcher);
  set_event_sink(&sink);

  bool running = true;
  const auto sub_close = sink.subscribe<window_close_requested_event>([&](const window_close_requested_event &e)
  {
    (void)e;
    std::printf("Close requested\n");
    running = false;
  });

  const auto sub_resize = sink.subscribe<window_resized_event>([&](const window_resized_event &e)
  {
    std::printf("Resized: %d x %d\n", e.width_px, e.height_px);
  });

  const auto sub_enter = sink.subscribe<window_enter_size_move_event>([&](const window_enter_size_move_event &)
  {
    std::printf("Enter size/move (interactive resize begins)\n");
  });

  const auto sub_exit = sink.subscribe<window_exit_size_move_event>([&](const window_exit_size_move_event &)
  {
    std::printf("Exit size/move (interactive resize ends)\n");
  });
  while (running && is_valid(w))
  {
    // Non-blocking: drain OS messages.
    pump_events();

    // Simulate a frame (60 Hz).
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    std::printf("Frame...\n");
  }

  destroy_window(w);
  return 0;
}
