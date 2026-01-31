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
