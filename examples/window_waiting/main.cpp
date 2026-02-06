// examples/window_waiting/main.cpp
// Demonstrates an "event-driven" loop: block until OS events arrive.

#include <catalyst/catalyst.hpp>
#include <catalyst/platform/window.hpp>

#include <cstdio>

int main()
{
  catalyst::catalyst_version_anchor();

  using namespace catalyst::platform;

  window_desc desc;
  desc.title = "Catalyst - window_waiting";
  desc.width_px = 800;
  desc.height_px = 450;
  desc.visible = true;

  window w = create_window(desc);
  if (!w)
  {
    std::fprintf(stderr, "Failed to create window\n");
    return 1;
  }

  std::printf("Waiting example: wait_events(INFINITE) then pump_events().\n");

  bool running = true;
  while (running && is_valid(w))
  {
    // Drain anything already queued first.
    event e;
    bool had_event = false;
    while (poll_event(e))
    {
      had_event = true;
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

    if (!running)
      break;

    // If nothing was queued, block until the OS has something for us.
    if (!had_event)
    {
      (void)wait_events(0xFFFFFFFFu); // INFINITE
      pump_events();
    }
    std::printf("Frame...\n");
  }

  destroy_window(w);
  return 0;
}
