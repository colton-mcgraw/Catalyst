/*
 * @file main.cpp
 * @brief Example of using the Catalyst platform library to create a window and wait for events.
 * @details This example demonstrates how to initialize the Catalyst platform library, create a window, and enter a main loop that waits for events. The example handles window close requests and resize events, printing relevant information to the console. It uses wait_events with an infinite timeout to block until events are available, followed by pump_events to process them. This serves as a basic template for using the Catalyst platform library in applications that require window management and event handling with an event-driven approach.
 * License: CDDL-1.0 (see LICENSE).
 */

#include <catalyst/catalyst.hpp>
#include <catalyst/core/dispatcher.hpp>
#include <catalyst/core/event_sink.hpp>
#include <catalyst/platform/window.hpp>

#include <cstdio>

int main()
{
  catalyst::catalyst_version_anchor();

  using namespace catalyst::platform;

  window_desc desc;
  desc.title = "Catalyst - window_waiting";
  desc.width_px = catalyst::ui::px(800.0f);
  desc.height_px = catalyst::ui::px(450.0f);
  desc.visible = true;

  window w = create_window(desc);
  if (!w)
  {
    std::fprintf(stderr, "Failed to create window\n");
    return 1;
  }

  std::printf("Waiting example: wait_events(INFINITE) then pump_events().\n");

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
    //std::printf("Resized: %d x %d\n", e.width_px, e.height_px);
  });

  const auto sub_enter = sink.subscribe<window_enter_size_move_event>([&](const window_enter_size_move_event &)
  {
    //std::printf("Enter size/move (interactive resize begins)\n");
  });

  const auto sub_exit = sink.subscribe<window_exit_size_move_event>([&](const window_exit_size_move_event &)
  {
    //std::printf("Exit size/move (interactive resize ends)\n");
  });
  while (running && is_valid(w))
  {
    (void)wait_events(0xFFFFFFFFu); // INFINITE
    pump_events();
    std::printf("Frame...\n");
  }

  destroy_window(w);
  return 0;
}
