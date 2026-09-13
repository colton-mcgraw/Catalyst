# Benchmarks, guarded on the modules they link. See examples/CMakeLists.txt for the same pattern and
# the reason for it.

function(catalyst_benchmark directory)
  foreach(_module ${ARGN})
    if(NOT TARGET catalyst::${_module})
      message(STATUS "Catalyst benchmark '${directory}' skipped: needs catalyst::${_module}")
      return()
    endif()
  endforeach()

  add_subdirectory(${directory})
endfunction()

catalyst_benchmark(events events)
catalyst_benchmark(json resource)
catalyst_benchmark(obj resource text)
catalyst_benchmark(render rendering platform)
