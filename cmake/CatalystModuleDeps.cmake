# Some modules genuinely cannot be built without others. catalyst::events stores its listeners in a
# type from catalyst::core; catalyst::resource's parsers are built on catalyst::text's scanner;
# catalyst::ui measures everything in catalyst::math vectors. Those are not preferences.
#
# Left unchecked, switching one off produces either a CMake error naming a target the reader never
# asked about ("Target catalyst_platform links to: catalyst::text") or, worse, a wall of missing
# includes. This says it in one line, and names the switch to flip.
#
# The bar for adding a dependency here is that the module does not compile without it -- not that it
# currently happens to include it. A dependency used only from a .cpp on one platform is still a
# real one: the module has to build everywhere.

function(catalyst_require_modules module)
  set(_missing "")

  foreach(_dependency ${ARGN})
    if(NOT TARGET catalyst::${_dependency})
      string(TOUPPER "${_dependency}" _upper)
      list(APPEND _missing "  - catalyst::${_dependency}  (CATALYST_BUILD_${_upper}=ON)")
    endif()
  endforeach()

  if(_missing STREQUAL "")
    return()
  endif()

  string(TOUPPER "${module}" _module_upper)
  list(JOIN _missing "\n" _missing_text)

  message(FATAL_ERROR
    "catalyst::${module} needs modules that are switched off:\n"
    "${_missing_text}\n\n"
    "Turn them on, or turn catalyst::${module} off with CATALYST_BUILD_${_module_upper}=OFF.\n")
endfunction()
