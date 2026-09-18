# The renderer bridge submits a ui::render_batch through catalyst::rendering; it is the one place
# the two modules meet, and the reason it is a separate target from catalyst_ui.
# Required, not optional: see cmake/CatalystModuleDeps.cmake.
include(CatalystModuleDeps)
catalyst_require_modules(ui_renderer)

add_library(catalyst_ui_renderer STATIC
  renderer.cpp
)

add_library(catalyst::ui_renderer ALIAS catalyst_ui_renderer)

target_include_directories(catalyst_ui_renderer
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# The generated <catalyst/config.hpp>.
target_link_libraries(catalyst_ui_renderer PUBLIC catalyst::config)

# ui/renderer.hpp names both a render_batch and the rendering handles it draws with.
target_link_libraries(catalyst_ui_renderer PUBLIC catalyst::ui catalyst::rendering)

# The shaders are checked in as SPIR-V in shaders.hpp so consumers need no Vulkan SDK to build.
# After editing shaders/ui.vert or shaders/ui.frag, regenerate with:
#   python scripts/embed_spirv.py --out src/ui/renderer/shaders.hpp --namespace catalyst::ui::detail::spirv \
#       ui_vertex=src/ui/renderer/shaders/ui.vert ui_fragment=src/ui/renderer/shaders/ui.frag
