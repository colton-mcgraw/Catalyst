# Some modules genuinely cannot be built without others. catalyst::events stores its listeners in a
# type from catalyst::core; catalyst::resource's parsers are built on catalyst::text's scanner;
# catalyst::ui measures everything in catalyst::math vectors. Those are not preferences.
#
# Left unchecked, switching one off produces either a CMake error naming a target the reader never
# asked about ("Target catalyst_platform links to: catalyst::text") or, worse, a wall of missing
# includes. This says it in one line, and names the switch to flip.
#
# The dependencies themselves are declared in cmake/CatalystModules.cmake, next to everything else
# about the module, and this reads them from there. They used to be repeated as arguments here,
# which is how catalyst::rendering came to link catalyst::logging without ever declaring it. The
# bar for adding one to the manifest is that the module does not compile without it -- not that it
# currently happens to include it. A dependency used only from a .cpp on one platform is still a
# real one: the module has to build everywhere.

include(CatalystModules)

# catalyst_require_modules(<module>)
# Fails the configure, in one sentence, if any module the manifest says <module> depends on has no
# target yet. Called at the top of the module's own CMakeLists.txt.
function(catalyst_require_modules module)
  if(ARGN)
    message(FATAL_ERROR
      "catalyst_require_modules(${module}): dependencies are no longer passed here. "
      "Declare them with DEPENDS in cmake/CatalystModules.cmake.")
  endif()
  if(NOT "${module}" IN_LIST CATALYST_MODULES)
    message(FATAL_ERROR
      "catalyst_require_modules(${module}): not a declared module. Add it to cmake/CatalystModules.cmake.")
  endif()

  set(_missing "")

  foreach(_dependency IN LISTS CATALYST_MODULE_${module}_DEPENDS)
    if(TARGET catalyst::${_dependency})
      continue()
    endif()

    set(_option "${CATALYST_MODULE_${_dependency}_OPTION}")
    set(_dir "src/${CATALYST_MODULE_${_dependency}_SOURCE_DIR}")

    # Distinguish "switched off" from "not in this checkout": the fix is different, and a sparse
    # checkout that forgot a foundation would otherwise be told to flip a switch that then fails
    # for the second reason.
    if(EXISTS "${PROJECT_SOURCE_DIR}/${_dir}/CMakeLists.txt")
      list(APPEND _missing "  - catalyst::${_dependency}  (${_option}=ON)")
    else()
      list(APPEND _missing
        "  - catalyst::${_dependency}  (not in this checkout: git sparse-checkout add ${_dir})")
    endif()
  endforeach()

  if(_missing STREQUAL "")
    return()
  endif()

  list(JOIN _missing "\n" _missing_text)

  message(FATAL_ERROR
    "catalyst::${module} needs modules that are switched off:\n"
    "${_missing_text}\n\n"
    "Turn them on, or turn catalyst::${module} off with ${CATALYST_MODULE_${module}_OPTION}=OFF.\n")
endfunction()
