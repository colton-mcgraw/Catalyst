# CMakeLists.txt for the Catalyst UI sample application.
# Builds a small retained UI tree against a real window: lays it out to the swapchain, paints it into a render_batch,
# draws that batch every frame through the ui::renderer bridge, and bridges the input module's mouse and keyboard
# events into the UI interaction state machine. Text is set from a TrueType file through a small stb_truetype-backed
# text_provider (font_provider.cpp) whose glyph atlas the sample uploads and registers with the renderer; when the
# build fetched no stb, or no font file is found at run time, the sample falls back to the module's null provider.
# Links the monolithic library when available, otherwise the platform, input, ui, ui_renderer, rendering, events and
# logging modules.
# License: MIT (see LICENSE).

add_executable(catalyst_ui_sample
  main.cpp
  font_provider.cpp
)

if(TARGET catalyst)
  target_link_libraries(catalyst_ui_sample PRIVATE catalyst)
else()
  target_link_libraries(catalyst_ui_sample PRIVATE
    catalyst_platform catalyst::input catalyst::ui catalyst::ui_renderer catalyst::rendering catalyst::events
    catalyst::logging)
endif()

# stb_truetype rides along with the stb_image fetch the resource module makes (cmake/stb.cmake); the target is global,
# so it is visible here whenever CATALYST_RESOURCE_STB was on. Without it the provider compiles to a stub that fails
# `open`, and the sample says so and draws text as boxes.
if(TARGET catalyst::stb)
  target_sources(catalyst_ui_sample PRIVATE stb_truetype_impl.cpp)
  target_link_libraries(catalyst_ui_sample PRIVATE catalyst::stb)
  target_compile_definitions(catalyst_ui_sample PRIVATE CATALYST_UI_SAMPLE_HAS_STB=1)
  # Third-party code compiled verbatim: its warnings are off for this one source file, as src/resource does for
  # stb_image. Not for MSVC, where /w on top of CMake's /W flag only earns a D9025.
  if(NOT MSVC)
    set_source_files_properties(stb_truetype_impl.cpp PROPERTIES COMPILE_OPTIONS "-w")
  endif()
else()
  message(STATUS "Catalyst example 'ui_sample': no catalyst::stb target, text will draw as boxes")
  target_compile_definitions(catalyst_ui_sample PRIVATE CATALYST_UI_SAMPLE_HAS_STB=0)
endif()

target_include_directories(catalyst_ui_sample PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(catalyst_ui_sample PRIVATE catalyst::config)
