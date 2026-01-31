add_library(catalyst_rendering STATIC
  rendering.cpp
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

if(_catalyst_rendering_backend STREQUAL "d3d12")
  if(NOT WIN32)
    message(FATAL_ERROR "CATALYST_RENDERING_BACKEND=d3d12 is only supported on Windows")
  endif()

  add_library(catalyst_rendering_backend_d3d12 STATIC
    d3d12/rendering_backend_d3d12.cpp
  )

  target_include_directories(catalyst_rendering_backend_d3d12
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
  )

  target_link_libraries(catalyst_rendering PRIVATE catalyst_rendering_backend_d3d12)
elseif(_catalyst_rendering_backend STREQUAL "vulkan")
  add_library(catalyst_rendering_backend_vulkan STATIC
    vulkan/rendering_backend_vulkan.cpp
  )

  target_include_directories(catalyst_rendering_backend_vulkan
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
  )

  target_link_libraries(catalyst_rendering PRIVATE catalyst_rendering_backend_vulkan)
elseif(_catalyst_rendering_backend STREQUAL "null")
  add_library(catalyst_rendering_backend_null STATIC
    null/rendering_backend_null.cpp
  )

  target_include_directories(catalyst_rendering_backend_null
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
  )

  target_link_libraries(catalyst_rendering PRIVATE catalyst_rendering_backend_null)
else()
  message(FATAL_ERROR "Unknown CATALYST_RENDERING_BACKEND: ${_catalyst_rendering_backend}")
endif()
