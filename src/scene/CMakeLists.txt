# Transforms, bounds, cameras and extraction are all expressed in catalyst::math types.
# Required, not optional: see cmake/CatalystModuleDeps.cmake.
include(CatalystModuleDeps)
catalyst_require_modules(scene math)

add_library(catalyst_scene STATIC
  scene.cpp
  transform.cpp
  bounds.cpp
  camera.cpp
  world.cpp
  render_list.cpp
)

add_library(catalyst::scene ALIAS catalyst_scene)

target_include_directories(catalyst_scene
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# The generated <catalyst/config.hpp>.
target_link_libraries(catalyst_scene PUBLIC catalyst::config)

# Every public header measures in catalyst::math.
target_link_libraries(catalyst_scene PUBLIC catalyst::math)

# scene/renderable.hpp builds mesh_id and material_id on rendering::resource_handle from
# <catalyst/rendering/types.hpp>. That header is constexpr-only, so this is an include-path
# dependency and not a link one -- the same arrangement catalyst::resource uses -- which is what
# keeps the world and extraction testable without a device. The renderer bridge that does link
# catalyst::rendering is a separate target; see docs/scene.md.
