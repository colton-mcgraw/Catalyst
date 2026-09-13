# Module targets live in subdirectories

# ---------------------------------------------------------------------------------------------
# Generated <catalyst/config.hpp>
#
# The module switches are a build-time choice and, until now, a consumer had no way to ask which way
# it went: <catalyst/catalyst.hpp> included all fourteen module headers unconditionally whatever the
# options said. This header is the answer, and it is what lets the umbrella include only what was
# built.
#
# The target is declared here because every module links it for its include directory. The file
# itself is written at the bottom of this file, once the rendering subdirectory has resolved which
# backend `auto` actually chose -- generating it here would record "none" every time.
# ---------------------------------------------------------------------------------------------

add_library(catalyst_config INTERFACE)
add_library(catalyst::config ALIAS catalyst_config)
target_include_directories(catalyst_config
  INTERFACE
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/generated>
    $<INSTALL_INTERFACE:include>
)


# Win32 plumbing shared by the platform, input and audio backends. Not a module: no build switch,
# and it is added before them so every backend can link it.
if(WIN32)
  add_subdirectory(win32)
endif()

# These four are added before everything else because other modules link them, and a module's
# dependency check (cmake/CatalystModuleDeps.cmake) runs when its subdirectory is added -- a
# foundation added later would not exist yet to be found. They are foundations rather than features:
# core holds vocabulary, events is the bus every module publishes on, text is the scanner the
# parsers are built on, math is the geometry input and ui measure in. All four are header-only.
if(CATALYST_BUILD_CORE)
  add_subdirectory(core)
endif()

if(CATALYST_BUILD_EVENTS)
  add_subdirectory(events)
endif()

if(CATALYST_BUILD_TEXT)
  add_subdirectory(text)
endif()

if(CATALYST_BUILD_MATH)
  add_subdirectory(math)
endif()

if(CATALYST_BUILD_ANIMATION)
  add_subdirectory(animation)
endif()

if(CATALYST_BUILD_AUDIO)
  add_subdirectory(audio)
endif()

if(CATALYST_BUILD_INPUT)
  add_subdirectory(input)
endif()

if(CATALYST_BUILD_LOGGING)
  add_subdirectory(logging)
endif()

if(CATALYST_BUILD_NET)
  add_subdirectory(net)
endif()

if(CATALYST_BUILD_PHYSICS)
  add_subdirectory(physics)
endif()

if(CATALYST_BUILD_PLATFORM)
  add_subdirectory(platform)
endif()

if(CATALYST_BUILD_RENDERING)
  add_subdirectory(rendering)
endif()

if(CATALYST_BUILD_RESOURCE)
  add_subdirectory(resource)
endif()

if(CATALYST_BUILD_SCENE)
  add_subdirectory(scene)
endif()

if(CATALYST_BUILD_UI)
  add_subdirectory(ui)
endif()

if(CATALYST_BUILD_UTILS)
  add_subdirectory(utils)
endif()

# Monolithic library that consumers can link against.
# It links whatever modules are enabled.
if(CATALYST_BUILD_ALL)
  add_library(catalyst STATIC
    catalyst.cpp
  )

  add_library(catalyst::catalyst ALIAS catalyst)
  add_library(catalyst::all ALIAS catalyst)

  target_include_directories(catalyst
    PUBLIC
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
  )

  if(TARGET catalyst_rendering)
    target_link_libraries(catalyst PUBLIC catalyst_rendering)
  endif()

  if(TARGET catalyst_resource)
    target_link_libraries(catalyst PUBLIC catalyst_resource)
  endif()

  if(TARGET catalyst_scene)
    target_link_libraries(catalyst PUBLIC catalyst_scene)
  endif()

  if(TARGET catalyst_ui)
    target_link_libraries(catalyst PUBLIC catalyst_ui)
  endif()

  if(TARGET catalyst_animation)
    target_link_libraries(catalyst PUBLIC catalyst_animation)
  endif()

  if(TARGET catalyst_audio)
    target_link_libraries(catalyst PUBLIC catalyst_audio)
  endif()

  if(TARGET catalyst_core)
    target_link_libraries(catalyst PUBLIC catalyst_core)
  endif()

  if(TARGET catalyst_events)
    target_link_libraries(catalyst PUBLIC catalyst_events)
  endif()

  if(TARGET catalyst_text)
    target_link_libraries(catalyst PUBLIC catalyst_text)
  endif()

  if(TARGET catalyst_input)
    target_link_libraries(catalyst PUBLIC catalyst_input)
  endif()

  if(TARGET catalyst_logging)
    target_link_libraries(catalyst PUBLIC catalyst_logging)
  endif()

  if(TARGET catalyst_math)
    target_link_libraries(catalyst PUBLIC catalyst_math)
  endif()

  if(TARGET catalyst_net)
    target_link_libraries(catalyst PUBLIC catalyst_net)
  endif()

  if(TARGET catalyst_physics)
    target_link_libraries(catalyst PUBLIC catalyst_physics)
  endif()

  if(TARGET catalyst_platform)
    target_link_libraries(catalyst PUBLIC catalyst_platform)
  endif()

  if(TARGET catalyst_utils)
    target_link_libraries(catalyst PUBLIC catalyst_utils)
  endif()
endif()


# ---------------------------------------------------------------------------------------------
# Write <catalyst/config.hpp>, now that every module has been added and the rendering backend has
# been resolved. See the catalyst_config target near the top of this file.
# ---------------------------------------------------------------------------------------------

foreach(_module ANIMATION AUDIO CORE EVENTS INPUT LOGGING MATH NET PHYSICS PLATFORM RENDERING RESOURCE SCENE TEXT UI UTILS)
  if(CATALYST_BUILD_${_module})
    set(CATALYST_HAS_${_module} 1)
  else()
    set(CATALYST_HAS_${_module} 0)
  endif()
endforeach()

if(CATALYST_RESOURCE_STB)
  set(CATALYST_HAS_STB_IMAGE 1)
else()
  set(CATALYST_HAS_STB_IMAGE 0)
endif()

# Set by src/rendering/CMakeLists.txt once `auto` has been resolved to a real backend. "none" when
# the rendering module is switched off entirely.
if(NOT _catalyst_rendering_backend_resolved)
  set(_catalyst_rendering_backend_resolved "none")
endif()

configure_file(
  ${PROJECT_SOURCE_DIR}/cmake/config.hpp.in
  ${PROJECT_BINARY_DIR}/generated/catalyst/config.hpp
  @ONLY
)


# ---------------------------------------------------------------------------------------------
# Install and export
#
# Every target above already declared $<INSTALL_INTERFACE:include> on its include directories. Until
# now nothing installed anything, so those generator expressions described an install that did not
# exist and the only way to consume Catalyst was add_subdirectory() on a source tree. This is the
# other half.
#
# Backend and support targets are exported alongside the modules they belong to. They are private
# implementation, but a static library's PRIVATE link still becomes an INTERFACE_LINK_LIBRARIES
# entry wrapped in $<LINK_ONLY:>, so the export set has to contain them or it will not generate.
# ---------------------------------------------------------------------------------------------

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(_catalyst_export_targets catalyst_config)
set(_catalyst_installed_modules "")

foreach(_module animation audio core events input logging math net physics platform rendering resource scene text ui utils)
  if(TARGET catalyst_${_module})
    list(APPEND _catalyst_export_targets catalyst_${_module})
    list(APPEND _catalyst_installed_modules ${_module})
  endif()
endforeach()

foreach(_support
    catalyst
    catalyst_win32_support
    catalyst_input_backend_win32 catalyst_input_backend_null
    catalyst_platform_backend_win32 catalyst_platform_backend_null
    catalyst_rendering_backend_vulkan catalyst_rendering_backend_d3d12 catalyst_rendering_backend_null)
  if(TARGET ${_support})
    list(APPEND _catalyst_export_targets ${_support})
  endif()
endforeach()

# NAMESPACE below prefixes the *target* name, so catalyst_audio would export as
# catalyst::catalyst_audio -- and the in-tree catalyst::audio is an ALIAS, which is not exported at
# all. EXPORT_NAME is what makes the installed spelling match the one every example and every piece
# of documentation already uses.
foreach(_target ${_catalyst_export_targets})
  if(_target STREQUAL "catalyst")
    set_target_properties(catalyst PROPERTIES EXPORT_NAME catalyst)
  else()
    string(REGEX REPLACE "^catalyst_" "" _export_name "${_target}")
    set_target_properties(${_target} PROPERTIES EXPORT_NAME ${_export_name})
  endif()
endforeach()

install(TARGETS ${_catalyst_export_targets}
  EXPORT CatalystTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# The hand-written headers, then the generated one, into the same tree so that
# <catalyst/config.hpp> sits beside <catalyst/audio/audio.hpp> exactly as it does in the build.
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/catalyst
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp"
)

install(FILES ${PROJECT_BINARY_DIR}/generated/catalyst/config.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/catalyst
)

install(EXPORT CatalystTargets
  FILE CatalystTargets.cmake
  NAMESPACE catalyst::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Catalyst
)

configure_package_config_file(
  ${PROJECT_SOURCE_DIR}/cmake/CatalystConfig.cmake.in
  ${PROJECT_BINARY_DIR}/CatalystConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Catalyst
)

# SameMinorVersion: Catalyst is pre-1.0, so the minor number is where breaking changes land.
write_basic_package_version_file(
  ${PROJECT_BINARY_DIR}/CatalystConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMinorVersion
)

install(FILES
    ${PROJECT_BINARY_DIR}/CatalystConfig.cmake
    ${PROJECT_BINARY_DIR}/CatalystConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Catalyst
)
