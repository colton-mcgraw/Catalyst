// examples/window_polling/main.cpp
// Demonstrates a classic "poll each frame" event pump.

#include <catalyst/catalyst.hpp>
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
  desc.width_px = 800;
  desc.height_px = 450;
  desc.visible = true;

  window w = create_window(desc);
  if (!w)
  {
    std::fprintf(stderr, "Failed to create window\n");
    return 1;
  }

  std::printf("Polling example: call pump_events() once per frame.\n");

  bool running = true;
  while (running && is_valid(w))
  {
    // Non-blocking: drain OS messages.
    pump_events();

    // Drain queued translated events.
    event e;
    while (poll_event(e))
    {
      switch (e.type)
      {
      case event_type::window_close_requested:
        std::printf("Close requested\n");
        running = false;
        break;
      case event_type::window_resized:
        std::printf("Resized: %d x %d\n", e.resized.width_px, e.resized.height_px);
        break;
      case event_type::window_enter_size_move:
        std::printf("Enter size/move (interactive resize begins)\n");
        break;
      case event_type::window_exit_size_move:
        std::printf("Exit size/move (interactive resize ends)\n");
        break;
      default:
        break;
      }
    }

    // Simulate a frame (60 Hz).
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    std::printf("Frame...\n");
  }

  destroy_window(w);
  return 0;
}
