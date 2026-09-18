# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026-Current Catalyst
#
# The one list of Catalyst modules.
#
# A module used to be named in nine places: the root option, the add_subdirectory order, the
# monolithic link list, the config.hpp defines, the install export, the tests tree, the README, CI
# and the umbrella header. That is how catalyst::rendering came to link catalyst::logging without
# declaring it, and how a new module could build in-tree yet be missing from the package. This file
# declares each module once; everything that used to repeat the list now reads it from here.
#
# It is loaded in two ways, and is written so that both work:
#   - by the root CMakeLists.txt, which then calls catalyst_module_options() to turn each
#     declaration into a CATALYST_BUILD_<MODULE> switch;
#   - by `cmake -P cmake/CatalystSparseCheckout.cmake`, in script mode, where no option() or
#     target exists. Nothing above the manifest section may need a project.
#
# Declaration order is build order. A module is added after everything it depends on, because the
# dependency check in cmake/CatalystModuleDeps.cmake runs when a module's subdirectory is added,
# and a foundation added later would not exist yet to be found.
#
#   catalyst_module(<name>
#     [PLACEHOLDER]          A module_name() and nothing else. Defaults OFF, and when OFF its headers
#                            are left out of the install so a package never carries an empty public
#                            namespace. Turn one on only while implementing it.
#     [DEPENDENT]            Switched off automatically when a dependency is off, instead of failing
#                            the configure. For bridges such as ui_renderer that only mean something
#                            when both sides exist.
#     [DEFAULT ON|OFF]       Default of the build switch. ON unless PLACEHOLDER.
#     [SOURCE_DIR <dir>]     Directory under src/, when it is not src/<name>.
#     [DEPENDS <module>...]  Modules this one does not compile without. Structural, not a preference:
#                            a PRIVATE link from one .cpp on one platform still counts, because the
#                            module has to build everywhere. Enforced by catalyst_require_modules()
#                            and followed by the sparse-checkout helper.
#     DESCRIPTION "<text>")  One line, used for the option help text and <catalyst/config.hpp>.

include_guard(GLOBAL)

set(CATALYST_MODULES "")

macro(catalyst_module name)
  cmake_parse_arguments(_cm "PLACEHOLDER;DEPENDENT" "DEFAULT;SOURCE_DIR;DESCRIPTION" "DEPENDS" ${ARGN})

  if(_cm_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "catalyst_module(${name}): unexpected arguments: ${_cm_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT _cm_DESCRIPTION)
    message(FATAL_ERROR "catalyst_module(${name}): DESCRIPTION is required")
  endif()
  if("${name}" IN_LIST CATALYST_MODULES)
    message(FATAL_ERROR "catalyst_module(${name}): declared twice")
  endif()
  foreach(_cm_dep IN LISTS _cm_DEPENDS)
    if(NOT "${_cm_dep}" IN_LIST CATALYST_MODULES)
      message(FATAL_ERROR
        "catalyst_module(${name}): depends on '${_cm_dep}', which is not declared above it. "
        "Declaration order is build order; a dependency has to come first.")
    endif()
  endforeach()

  if(NOT _cm_SOURCE_DIR)
    set(_cm_SOURCE_DIR "${name}")
  endif()
  if(NOT DEFINED _cm_DEFAULT)
    if(_cm_PLACEHOLDER)
      set(_cm_DEFAULT OFF)
    else()
      set(_cm_DEFAULT ON)
    endif()
  endif()

  string(TOUPPER "${name}" _cm_upper)

  list(APPEND CATALYST_MODULES "${name}")
  set(CATALYST_MODULE_${name}_OPTION "CATALYST_BUILD_${_cm_upper}")
  set(CATALYST_MODULE_${name}_SOURCE_DIR "${_cm_SOURCE_DIR}")
  set(CATALYST_MODULE_${name}_DEPENDS "${_cm_DEPENDS}")
  set(CATALYST_MODULE_${name}_DEFAULT "${_cm_DEFAULT}")
  set(CATALYST_MODULE_${name}_PLACEHOLDER "${_cm_PLACEHOLDER}")
  set(CATALYST_MODULE_${name}_DEPENDENT "${_cm_DEPENDENT}")
  set(CATALYST_MODULE_${name}_DESCRIPTION "${_cm_DESCRIPTION}")

  unset(_cm_PLACEHOLDER)
  unset(_cm_DEPENDENT)
  unset(_cm_DEFAULT)
  unset(_cm_SOURCE_DIR)
  unset(_cm_DESCRIPTION)
  unset(_cm_DEPENDS)
  unset(_cm_UNPARSED_ARGUMENTS)
  unset(_cm_KEYWORDS_MISSING_VALUES)
  unset(_cm_dep)
  unset(_cm_upper)
endmacro()


# ---------------------------------------------------------------------------------------------
# The manifest
# ---------------------------------------------------------------------------------------------

# Foundations first. All four are header-only; every other module builds on at least one of them.
catalyst_module(core
  DESCRIPTION "Shared vocabulary: the version string and what more than one module needs")
catalyst_module(events DEPENDS core
  DESCRIPTION "The bus every other module publishes on (header-only)")
catalyst_module(text
  DESCRIPTION "UTF-8 encoding and decoding, and the scanner the parsers are built on (header-only)")
catalyst_module(math
  DESCRIPTION "Vectors, matrices, quaternions, transforms, fractions, geometry (header-only)")

catalyst_module(animation PLACEHOLDER
  DESCRIPTION "Not implemented")
catalyst_module(audio DEPENDS events
  DESCRIPTION "Device enumeration, streams, mixing, offline rendering -- WASAPI and ASIO backends")
catalyst_module(input DEPENDS events math
  DESCRIPTION "Keyboard, text, mouse, gamepads, action maps, calibration")
catalyst_module(logging DEPENDS core
  DESCRIPTION "Levels, filters, middleware, routing and sinks")
catalyst_module(net PLACEHOLDER
  DESCRIPTION "Not implemented")
catalyst_module(physics PLACEHOLDER
  DESCRIPTION "Not implemented")
catalyst_module(platform DEPENDS events text
  DESCRIPTION "Windows, monitors, the event loop -- Win32 backend")
# logging is a PRIVATE link (src/rendering/detail_log.hpp) and was never declared here, so
# -DCATALYST_BUILD_LOGGING=OFF used to fail on "target catalyst::logging not found" instead of a
# sentence naming the switch.
catalyst_module(rendering DEPENDS events logging
  DESCRIPTION "Device, queues, timeline sync, buffers, textures, pipelines, swapchain, transfer -- Vulkan backend")
catalyst_module(resource DEPENDS events text
  DESCRIPTION "VFS, loaders, URIs, JSON, CSV, OBJ, images, mip generation")
catalyst_module(scene DEPENDS math
  DESCRIPTION "Entities with a transform hierarchy and components, cameras, lights, culling, extraction")
catalyst_module(ui DEPENDS math events text
  DESCRIPTION "Retained tree of styled nodes, flexbox layout, painting into a draw list, hit testing, text layout")
# The bridge that submits catalyst::ui draw lists through catalyst::rendering. DEPENDENT so that
# switching either side off does not fail the configure; it simply goes with them.
catalyst_module(ui_renderer DEPENDENT SOURCE_DIR ui/renderer DEPENDS ui rendering
  DESCRIPTION "The bridge that draws catalyst::ui batches through catalyst::rendering")
catalyst_module(utils PLACEHOLDER
  DESCRIPTION "Not implemented")


# ---------------------------------------------------------------------------------------------
# Helpers over the manifest. Usable in script mode too.
# ---------------------------------------------------------------------------------------------

# catalyst_module_closure(<out-var> <module>...)
# The given modules plus everything they depend on, transitively, in declaration (= build) order.
# Unknown names are an error naming the manifest.
function(catalyst_module_closure out)
  set(_wanted "")
  foreach(_m IN LISTS ARGN)
    if(NOT "${_m}" IN_LIST CATALYST_MODULES)
      message(FATAL_ERROR
        "'${_m}' is not a Catalyst module. The modules are: ${CATALYST_MODULES} "
        "(cmake/CatalystModules.cmake).")
    endif()
    list(APPEND _wanted "${_m}")
  endforeach()

  # Fixed point: keep adding dependencies until nothing new appears. The list is short enough that
  # the simplest loop is the right one.
  set(_changed TRUE)
  while(_changed)
    set(_changed FALSE)
    foreach(_m IN LISTS _wanted)
      foreach(_d IN LISTS CATALYST_MODULE_${_m}_DEPENDS)
        if(NOT "${_d}" IN_LIST _wanted)
          list(APPEND _wanted "${_d}")
          set(_changed TRUE)
        endif()
      endforeach()
    endforeach()
  endwhile()

  set(_ordered "")
  foreach(_m IN LISTS CATALYST_MODULES)
    if("${_m}" IN_LIST _wanted)
      list(APPEND _ordered "${_m}")
    endif()
  endforeach()

  set(${out} "${_ordered}" PARENT_SCOPE)
endfunction()


# ---------------------------------------------------------------------------------------------
# Project-mode only: the build switches.
# ---------------------------------------------------------------------------------------------

# catalyst_module_options()
# One CATALYST_BUILD_<MODULE> option per declaration. A macro rather than a function on purpose:
# cmake_dependent_option() records a forced-off value as a normal variable in the calling scope,
# and from inside a function that variable would be lost on return, leaving a stale ON in the
# cache from an earlier configure.
#
# A module whose sources are not in the checkout defaults OFF whatever the manifest says: that is
# what makes a sparse checkout of a few modules configure without touching a switch. Asking for
# such a module explicitly is an error that names the directory to add.
macro(catalyst_module_options)
  include(CMakeDependentOption)

  foreach(_cmo_module IN LISTS CATALYST_MODULES)
    set(_cmo_option "${CATALYST_MODULE_${_cmo_module}_OPTION}")
    set(_cmo_dir "src/${CATALYST_MODULE_${_cmo_module}_SOURCE_DIR}")
    set(_cmo_help "Build Catalyst ${_cmo_module} module: ${CATALYST_MODULE_${_cmo_module}_DESCRIPTION}")

    if(EXISTS "${PROJECT_SOURCE_DIR}/${_cmo_dir}/CMakeLists.txt")
      set(_cmo_present TRUE)
      set(_cmo_default "${CATALYST_MODULE_${_cmo_module}_DEFAULT}")
    else()
      set(_cmo_present FALSE)
      set(_cmo_default OFF)
    endif()

    if(CATALYST_MODULE_${_cmo_module}_PLACEHOLDER)
      string(APPEND _cmo_help " (PLACEHOLDER: no implementation yet)")
    endif()

    if(CATALYST_MODULE_${_cmo_module}_DEPENDENT)
      set(_cmo_conditions "")
      foreach(_cmo_dep IN LISTS CATALYST_MODULE_${_cmo_module}_DEPENDS)
        list(APPEND _cmo_conditions "${CATALYST_MODULE_${_cmo_dep}_OPTION}")
      endforeach()
      cmake_dependent_option(${_cmo_option} "${_cmo_help}" ${_cmo_default} "${_cmo_conditions}" OFF)
    else()
      option(${_cmo_option} "${_cmo_help}" ${_cmo_default})
    endif()

    if(${_cmo_option} AND NOT _cmo_present)
      message(FATAL_ERROR
        "${_cmo_option}=ON, but ${_cmo_dir}/ is not in this checkout.\n"
        "If this is a sparse checkout, add the module's sources:\n"
        "  git sparse-checkout add ${_cmo_dir}\n"
        "or let cmake/CatalystSparseCheckout.cmake work out the full set, or switch it off:\n"
        "  -D${_cmo_option}=OFF\n")
    endif()
  endforeach()

  unset(_cmo_module)
  unset(_cmo_option)
  unset(_cmo_dir)
  unset(_cmo_help)
  unset(_cmo_present)
  unset(_cmo_default)
  unset(_cmo_conditions)
  unset(_cmo_dep)
endmacro()
