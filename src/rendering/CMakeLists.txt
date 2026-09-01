add_library(catalyst_rendering STATIC
  rendering.cpp
  device.cpp
  buffer.cpp
  shader.cpp
  texture.cpp
  pipeline.cpp
  swapchain.cpp
  command.cpp
)

add_library(catalyst::rendering ALIAS catalyst_rendering)

target_include_directories(catalyst_rendering
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

set(CATALYST_RENDERING_BACKEND "auto" CACHE STRING "Rendering backend (auto|d3d12|vulkan|null)")
set_property(CACHE CATALYST_RENDERING_BACKEND PROPERTY STRINGS auto d3d12 vulkan null)

set(_catalyst_rendering_backend "${CATALYST_RENDERING_BACKEND}")
if(_catalyst_rendering_backend STREQUAL "auto")
  if(WIN32)
    set(_catalyst_rendering_backend "d3d12")
  else()
    set(_catalyst_rendering_backend "vulkan")
  endif()
endif()

# CPU-side bookkeeping implementation of the resource contract (src/rendering/detail_backend.hpp).
# It is the resource layer of the null backend and stands in for backends whose GPU resource layer
# has not been written yet, so the public API links and is testable on every configuration.
set(_catalyst_rendering_null_resources ${CMAKE_CURRENT_SOURCE_DIR}/null/null_resources.cpp)

if(_catalyst_rendering_backend STREQUAL "d3d12")
  if(NOT WIN32)
    message(FATAL_ERROR "CATALYST_RENDERING_BACKEND=d3d12 is only supported on Windows")
  endif()

  add_library(catalyst_rendering_backend_d3d12 STATIC
    d3d12/rendering_backend_d3d12.cpp
    # TODO(d3d12): replace with a real resource implementation.
    ${_catalyst_rendering_null_resources}
  )

  target_include_directories(catalyst_rendering_backend_d3d12
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${PROJECT_SOURCE_DIR}/include
  )

  target_link_libraries(catalyst_rendering PRIVATE catalyst_rendering_backend_d3d12)
elseif(_catalyst_rendering_backend STREQUAL "vulkan")
  # Headers and the loader import library come from the Vulkan SDK (VULKAN_SDK env var) or the system package.
  find_package(Vulkan REQUIRED)

  add_library(catalyst_rendering_backend_vulkan STATIC
    vulkan/rendering_backend_vulkan.cpp
    vulkan/vulkan_device.cpp
    vulkan/vulkan_memory.cpp
    vulkan/vulkan_buffer.cpp
    vulkan/vulkan_shader.cpp
    vulkan/vulkan_texture.cpp
    vulkan/vulkan_pipeline.cpp
    vulkan/vulkan_swapchain.cpp
    vulkan/vulkan_command.cpp
  )

  target_include_directories(catalyst_rendering_backend_vulkan
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${PROJECT_SOURCE_DIR}/include
  )

  target_link_libraries(catalyst_rendering_backend_vulkan PUBLIC Vulkan::Vulkan)

  target_link_libraries(catalyst_rendering PRIVATE catalyst_rendering_backend_vulkan)
elseif(_catalyst_rendering_backend STREQUAL "null")
  add_library(catalyst_rendering_backend_null STATIC
    null/rendering_backend_null.cpp
    ${_catalyst_rendering_null_resources}
  )

  target_include_directories(catalyst_rendering_backend_null
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${PROJECT_SOURCE_DIR}/include
  )

  target_link_libraries(catalyst_rendering PRIVATE catalyst_rendering_backend_null)
else()
  message(FATAL_ERROR "Unknown CATALYST_RENDERING_BACKEND: ${_catalyst_rendering_backend}")
endif()
