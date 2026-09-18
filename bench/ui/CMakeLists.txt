# CMakeLists.txt for the Catalyst UI benchmarks.
# The benchmarks build a dashboard-style screen out of the public catalyst::ui API and time the CPU
# side of a frame: layout, paint into a render_batch, text layout and hit testing. They never link
# the rendering module, so they measure the draw list the UI produces, not any backend.
# License: MIT (see LICENSE).

if(NOT TARGET catalyst::ui)
  message(STATUS "Catalyst UI benchmarks skipped: they need CATALYST_BUILD_UI")
  return()
endif()

add_executable(catalyst_bench_ui
  ui.cpp
)

target_link_libraries(catalyst_bench_ui
  PRIVATE
    catalyst::ui
)

target_include_directories(catalyst_bench_ui
  PRIVATE
    ${PROJECT_SOURCE_DIR}/bench
)
