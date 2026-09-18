# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026-Current Catalyst
#
# Works out which directories a sparse checkout needs for a set of modules, and optionally applies
# it. Run in script mode from the repository root:
#
#   cmake -DMODULES="audio;logging" -P cmake/CatalystSparseCheckout.cmake
#   cmake -DMODULES="audio;logging" -DAPPLY=ON -P cmake/CatalystSparseCheckout.cmake
#   cmake -DMODULES="ui" -DEXTRA="tests;examples" -P cmake/CatalystSparseCheckout.cmake
#
#   MODULES  The modules you want. Their dependencies (cmake/CatalystModules.cmake) are added.
#   EXTRA    Top-level directories to add as well: tests, examples, bench, docs. For tests, only
#            the suites of the chosen modules are taken.
#   APPLY    ON runs `git sparse-checkout set` with the result. Off prints it.
#
# Why include/ is taken whole: public headers include across modules -- platform/window.hpp
# includes ui and input headers, resource includes rendering's format -- and the include tree is
# small. Sources are what a checkout can leave out, one module at a time: the build switches
# default OFF for any module whose src/ directory is absent (see catalyst_module_options), so the
# result of this script configures without touching a switch.
#
# src/win32 is always included. It is the Win32 plumbing the platform, input and audio backends
# link, five files, and leaving it out of a checkout that is later configured on Windows would
# fail in the middle of a backend's link line.

cmake_minimum_required(VERSION 3.23)

include("${CMAKE_CURRENT_LIST_DIR}/CatalystModules.cmake")

if(NOT MODULES)
  message(FATAL_ERROR
    "Name the modules you want, e.g.\n"
    "  cmake -DMODULES=\"audio;logging\" -P cmake/CatalystSparseCheckout.cmake\n"
    "Modules: ${CATALYST_MODULES}")
endif()

catalyst_module_closure(_closure ${MODULES})

set(_paths cmake include src/win32)
foreach(_m IN LISTS _closure)
  list(APPEND _paths "src/${CATALYST_MODULE_${_m}_SOURCE_DIR}")
endforeach()

foreach(_extra IN LISTS EXTRA)
  if(_extra STREQUAL "tests")
    foreach(_m IN LISTS _closure)
      list(APPEND _paths "tests/${_m}")
    endforeach()
  elseif(_extra MATCHES "^(examples|bench|docs)$")
    list(APPEND _paths "${_extra}")
  else()
    message(FATAL_ERROR "EXTRA: '${_extra}' is not one of tests, examples, bench, docs")
  endif()
endforeach()

list(REMOVE_DUPLICATES _paths)
list(JOIN _closure " " _closure_text)
list(JOIN _paths " " _paths_text)

message(STATUS "Modules (with dependencies): ${_closure_text}")
message(STATUS "Sparse-checkout paths:       ${_paths_text}")

get_filename_component(_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(APPLY)
  execute_process(
    COMMAND git -C "${_root}" sparse-checkout set --cone ${_paths}
    RESULT_VARIABLE _result
  )
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR "git sparse-checkout failed (${_result}). Is ${_root} a git checkout?")
  endif()
  message(STATUS "Applied. Configure as usual; modules outside the checkout default OFF.")
else()
  message(STATUS "To apply it:\n  git sparse-checkout set --cone ${_paths_text}\nor re-run with -DAPPLY=ON.")
endif()
