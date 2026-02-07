# CMakeLists.txt for the Catalyst sandbox example application.
# This example demonstrates how to set up a simple application that links against the Catalyst libraries. It includes the main source file and links against either the monolithic catalyst library or the individual modules, depending on how the project is configured. The example also ensures that the include directories are set up correctly to access the Catalyst headers.
# License: CDDL-1.0 (see LICENSE).

add_executable(catalyst_sandbox
  main.cpp
)

# Prefer the monolithic library if enabled; otherwise link whichever modules exist.
if(TARGET catalyst)
  target_link_libraries(catalyst_sandbox PRIVATE catalyst)
else()
  if(TARGET catalyst_rendering)
    target_link_libraries(catalyst_sandbox PRIVATE catalyst_rendering)
  endif()
  if(TARGET catalyst_ui)
    target_link_libraries(catalyst_sandbox PRIVATE catalyst_ui)
  endif()
endif()

target_include_directories(catalyst_sandbox PRIVATE ${PROJECT_SOURCE_DIR}/include)
