add_library(catalyst_audio STATIC
  engine.cpp
)

add_library(catalyst::audio ALIAS catalyst_audio)

target_include_directories(catalyst_audio
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

set(CATALYST_AUDIO_BACKEND "auto" CACHE STRING "Audio backend (auto|win32|null)")
set_property(CACHE CATALYST_AUDIO_BACKEND PROPERTY STRINGS auto win32 null)

set(_catalyst_audio_backend "${CATALYST_AUDIO_BACKEND}")
if(_catalyst_audio_backend STREQUAL "auto")
  if(WIN32)
    set(_catalyst_audio_backend "win32")
  else()
    set(_catalyst_audio_backend "null")
  endif()
endif()

if(_catalyst_audio_backend STREQUAL "win32")
  if(NOT WIN32)
    message(FATAL_ERROR "CATALYST_AUDIO_BACKEND=win32 is only supported on Windows")
  endif()

  add_library(catalyst_audio_backend_win32 STATIC
    win32/audio_backend_win32.cpp
  )

  target_include_directories(catalyst_audio_backend_win32
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${PROJECT_SOURCE_DIR}/include
  )

  # Win32 audio builds always include ASIO (SDK-free minimal loader).
  target_sources(catalyst_audio_backend_win32 PRIVATE
    asio/asio_loader.cpp
    asio/asio_backend_win32.cpp
    wasapi/wasapi_backend_win32.cpp
  )

  target_link_libraries(catalyst_audio_backend_win32 PRIVATE Ole32 Uuid)

  target_link_libraries(catalyst_audio PRIVATE catalyst_audio_backend_win32)
elseif(_catalyst_audio_backend STREQUAL "null")
  add_library(catalyst_audio_backend_null STATIC
    null/audio_backend_null.cpp
  )

  target_include_directories(catalyst_audio_backend_null
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${PROJECT_SOURCE_DIR}/include
  )

  target_link_libraries(catalyst_audio PRIVATE catalyst_audio_backend_null)
else()
  message(FATAL_ERROR "Unknown CATALYST_AUDIO_BACKEND: ${_catalyst_audio_backend}")
endif()
