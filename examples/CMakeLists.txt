# CMakeLists.txt for the Catalyst example applications.
#
# Each example is guarded on the modules it actually links, so a subset build skips the ones it
# cannot build instead of failing at link time. A skipped example says so at configure time rather
# than disappearing silently.
# License: MIT (see LICENSE).

function(catalyst_example directory)
  foreach(_module ${ARGN})
    if(NOT TARGET catalyst::${_module})
      message(STATUS "Catalyst example '${directory}' skipped: needs catalyst::${_module}")
      return()
    endif()
  endforeach()

  add_subdirectory(${directory})
endfunction()

catalyst_example(sandbox core logging resource)
catalyst_example(window_polling platform logging events)
catalyst_example(window_waiting platform logging events)
catalyst_example(input_events input platform logging events text)
catalyst_example(input_actions input platform logging events)
catalyst_example(rendering_basics rendering platform logging)
catalyst_example(audio_playback audio logging)
