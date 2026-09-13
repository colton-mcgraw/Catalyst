# The module publishes to a catalyst::events::bus and describes positions with catalyst::math vectors.
# Required, not optional: see cmake/CatalystModuleDeps.cmake.
include(CatalystModuleDeps)
catalyst_require_modules(input events math)

add_library(catalyst_input STATIC
  action.cpp
  action_map.cpp
  binding.cpp
  calibration.cpp
  device.cpp
  gamepad.cpp
  input.cpp
  keyboard.cpp
  registry.cpp
  state.cpp
  context.cpp
)

add_library(catalyst::input ALIAS catalyst_input)

target_include_directories(catalyst_input
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# The generated <catalyst/config.hpp>.
target_link_libraries(catalyst_input PUBLIC catalyst::config)

# The module publishes to a catalyst::events::bus and describes positions with catalyst::math
# vectors. Both are header-only INTERFACE targets: no link cost, and both appear where they belong
# in the build graph.
target_link_libraries(catalyst_input PUBLIC catalyst::events catalyst::math)

set(CATALYST_INPUT_BACKEND "auto" CACHE STRING "Input backend (auto|win32|null)")
set_property(CACHE CATALYST_INPUT_BACKEND PROPERTY STRINGS auto win32 null)

set(_catalyst_input_backend "${CATALYST_INPUT_BACKEND}")
if(_catalyst_input_backend STREQUAL "auto")
  if(WIN32)
    set(_catalyst_input_backend "win32")
  else()
    set(_catalyst_input_backend "null")
  endif()
endif()

if(_catalyst_input_backend STREQUAL "win32")
  if(NOT WIN32)
    message(FATAL_ERROR "CATALYST_INPUT_BACKEND=win32 is only supported on Windows")
  endif()

  add_library(catalyst_input_backend_win32 STATIC
    win32/input_backend_win32.cpp
  )

  target_include_directories(catalyst_input_backend_win32
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${PROJECT_SOURCE_DIR}/include
  )

  # XInput 1.4 (Windows 8+) for gamepads.
  target_link_libraries(catalyst_input_backend_win32 PRIVATE xinput)

  # The <windows.h> include, shared with the platform and audio backends.
  target_link_libraries(catalyst_input_backend_win32 PRIVATE catalyst::win32_support)

  target_link_libraries(catalyst_input PRIVATE catalyst_input_backend_win32)
elseif(_catalyst_input_backend STREQUAL "null")
  add_library(catalyst_input_backend_null STATIC
    null/input_backend_null.cpp
  )

  target_include_directories(catalyst_input_backend_null
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${PROJECT_SOURCE_DIR}/include
  )

  target_link_libraries(catalyst_input PRIVATE catalyst_input_backend_null)
else()
  message(FATAL_ERROR "Unknown CATALYST_INPUT_BACKEND: ${_catalyst_input_backend}")
endif()
