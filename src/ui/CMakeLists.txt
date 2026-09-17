# Geometry and layout are expressed with the vector and rectangle types from catalyst::math;
# ui/events.hpp tags its events for, and ui/interaction.hpp publishes on, a catalyst::events::bus;
# text.cpp walks labels by code point with catalyst::text's UTF-8 decoder.
# Required, not optional: see cmake/CatalystModuleDeps.cmake.
include(CatalystModuleDeps)
catalyst_require_modules(ui math events text)

add_library(catalyst_ui STATIC
  ui.cpp
  tree.cpp
  layout.cpp
  batch.cpp
  paint.cpp
  hit_test.cpp
  interaction.cpp
  text.cpp
)

add_library(catalyst::ui ALIAS catalyst_ui)

target_include_directories(catalyst_ui
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# The generated <catalyst/config.hpp>.
target_link_libraries(catalyst_ui PUBLIC catalyst::config)

# Geometry and layout are expressed with the vector and rectangle types from catalyst::math.
target_link_libraries(catalyst_ui PUBLIC catalyst::math)

# ui/events.hpp derives every UI event from events::tagged, and ui/interaction.hpp holds a bus.
target_link_libraries(catalyst_ui PUBLIC catalyst::events)

# Only text.cpp includes the UTF-8 decoder; no public UI header does.
target_link_libraries(catalyst_ui PRIVATE catalyst::text)

# What is deliberately not here: catalyst::rendering. The module emits a render_batch and never
# submits one; the target that does, catalyst_ui_renderer, is Tier 5 in docs/ui.md and links both.
