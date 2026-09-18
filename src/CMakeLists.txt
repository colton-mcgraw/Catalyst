# Module targets live in subdirectories. Which subdirectories, in what order, and what each one is
# called, all comes from the manifest in cmake/CatalystModules.cmake -- this file only walks it.

# ---------------------------------------------------------------------------------------------
# Generated <catalyst/config.hpp>
#
# The module switches are a build-time choice and, until now, a consumer had no way to ask which way
# it went: <catalyst/catalyst.hpp> included every module header unconditionally whatever the
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
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/win32/CMakeLists.txt")
    message(FATAL_ERROR
      "src/win32/ is not in this checkout, and the Win32 backends link it.\n"
      "If this is a sparse checkout: git sparse-checkout add src/win32\n"
      "Or build the null backends: -DCATALYST_PLATFORM_BACKEND=null -DCATALYST_INPUT_BACKEND=null "
      "-DCATALYST_AUDIO_BACKEND=null\n")
  endif()
  add_subdirectory(win32)
endif()

# Manifest order is build order: the foundations (core, events, text, math) come first because
# other modules link them and a module's dependency check runs when its subdirectory is added;
# ui_renderer comes after both ui and rendering for the same reason.
foreach(_module IN LISTS CATALYST_MODULES)
  if(${CATALYST_MODULE_${_module}_OPTION})
    add_subdirectory(${CATALYST_MODULE_${_module}_SOURCE_DIR})
  endif()
endforeach()

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

  foreach(_module IN LISTS CATALYST_MODULES)
    if(TARGET catalyst_${_module})
      target_link_libraries(catalyst PUBLIC catalyst_${_module})
    endif()
  endforeach()
endif()


# ---------------------------------------------------------------------------------------------
# Write <catalyst/config.hpp>, now that every module has been added and the rendering backend has
# been resolved. See the catalyst_config target near the top of this file.
# ---------------------------------------------------------------------------------------------

set(CATALYST_CONFIG_MODULE_DEFINES "")
foreach(_module IN LISTS CATALYST_MODULES)
  string(TOUPPER "${_module}" _upper)
  if(${CATALYST_MODULE_${_module}_OPTION})
    set(_value 1)
  else()
    set(_value 0)
  endif()
  string(APPEND CATALYST_CONFIG_MODULE_DEFINES
    "/** @brief catalyst::${_module}: ${CATALYST_MODULE_${_module}_DESCRIPTION}. */\n"
    "#define CATALYST_HAS_${_upper} ${_value}\n")
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

# The umbrella header is hand-written, and it is the one inventory the manifest cannot generate
# without owning a public header. So it is checked instead: a module declared here but not guarded
# there would build, install, and then be missing from <catalyst/catalyst.hpp>.
file(STRINGS ${PROJECT_SOURCE_DIR}/include/catalyst/catalyst.hpp _umbrella_guards REGEX "^#if CATALYST_HAS_")
foreach(_module IN LISTS CATALYST_MODULES)
  string(TOUPPER "${_module}" _upper)
  if(NOT "#if CATALYST_HAS_${_upper}" IN_LIST _umbrella_guards)
    message(AUTHOR_WARNING
      "include/catalyst/catalyst.hpp has no '#if CATALYST_HAS_${_upper}' block for catalyst::${_module}. "
      "Every module in cmake/CatalystModules.cmake should be included there behind its guard.")
  endif()
endforeach()


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
set(_catalyst_header_excludes "")

foreach(_module IN LISTS CATALYST_MODULES)
  if(TARGET catalyst_${_module})
    list(APPEND _catalyst_export_targets catalyst_${_module})
    list(APPEND _catalyst_installed_modules ${_module})
  elseif(CATALYST_MODULE_${_module}_PLACEHOLDER)
    # A placeholder that was not built ships no header either: an empty namespace in an installed
    # header is a public API by accident, and this is where the release boundary is drawn. Headers
    # of a real module that was switched off are still installed, because public headers include
    # across modules (platform/window.hpp includes ui and input headers) and <catalyst/config.hpp>
    # is how a consumer tells what is actually linkable.
    list(APPEND _catalyst_header_excludes REGEX "/include/catalyst/${_module}(/|$)" EXCLUDE)
  endif()
endforeach()

foreach(_support
    catalyst
    catalyst_win32_support
    catalyst_input_backend_win32 catalyst_input_backend_null
    catalyst_platform_backend_win32 catalyst_platform_backend_null
    catalyst_rendering_backend_vulkan catalyst_rendering_backend_d3d12
    catalyst_rendering_backend_metal catalyst_rendering_backend_null)
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
# The EXCLUDE expressions come first because the first match decides.
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/catalyst
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING
    ${_catalyst_header_excludes}
    PATTERN "*.hpp"
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

# SameMinorVersion: Catalyst is pre-1.0, so the minor number is where breaking changes land. The
# policy is written down in README.md ("Versioning and compatibility") and CHANGELOG.md.
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
