add_library(catalyst_platform STATIC
  platform.cpp
)

add_library(catalyst::platform ALIAS catalyst_platform)

target_include_directories(catalyst_platform
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

set(CATALYST_PLATFORM_BACKEND "auto" CACHE STRING "Platform backend (auto|win32|null)")
set_property(CACHE CATALYST_PLATFORM_BACKEND PROPERTY STRINGS auto win32 null)

set(_catalyst_platform_backend "${CATALYST_PLATFORM_BACKEND}")
if(_catalyst_platform_backend STREQUAL "auto")
  if(WIN32)
    set(_catalyst_platform_backend "win32")
  else()
    set(_catalyst_platform_backend "null")
  endif()
endif()

if(_catalyst_platform_backend STREQUAL "win32")
  if(NOT WIN32)
    message(FATAL_ERROR "CATALYST_PLATFORM_BACKEND=win32 is only supported on Windows")
  endif()

  add_library(catalyst_platform_backend_win32 STATIC
    win32/platform_backend_win32.cpp
  )

  target_include_directories(catalyst_platform_backend_win32
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
  )

  target_link_libraries(catalyst_platform PRIVATE catalyst_platform_backend_win32)
elseif(_catalyst_platform_backend STREQUAL "null")
  add_library(catalyst_platform_backend_null STATIC
    null/platform_backend_null.cpp
  )

  target_include_directories(catalyst_platform_backend_null
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
  )

  target_link_libraries(catalyst_platform PRIVATE catalyst_platform_backend_null)
else()
  message(FATAL_ERROR "Unknown CATALYST_PLATFORM_BACKEND: ${_catalyst_platform_backend}")
endif()
