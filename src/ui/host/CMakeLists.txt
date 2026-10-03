# The shell around the renderer bridge: ui::host (a tree, its interaction, the renderer and the glyph
# atlas upload), ui::input_bridge (input module events into the interaction), and ui::app (a window,
# a device and the frame loop around one host). A separate target because it is the one place the
# UI meets the window and input modules; catalyst_ui stays free of both, and catalyst_ui_renderer of
# everything but rendering.
# Required, not optional: see cmake/CatalystModuleDeps.cmake.
include(CatalystModuleDeps)
catalyst_require_modules(ui_host)

add_library(catalyst_ui_host STATIC
  host.cpp
  input_bridge.cpp
  app.cpp
)

add_library(catalyst::ui_host ALIAS catalyst_ui_host)

target_include_directories(catalyst_ui_host
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# The generated <catalyst/config.hpp>.
target_link_libraries(catalyst_ui_host PUBLIC catalyst::config)

# ui/host.hpp holds a renderer; ui/input_bridge.hpp names input events; ui/app.hpp holds a window
# and an input context.
target_link_libraries(catalyst_ui_host PUBLIC catalyst::ui_renderer catalyst::input catalyst::platform)
