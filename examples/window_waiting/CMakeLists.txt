add_executable(catalyst_window_waiting
  main.cpp
)

# Prefer the monolithic library if enabled; otherwise link the platform module.
if(TARGET catalyst)
  target_link_libraries(catalyst_window_waiting PRIVATE catalyst)
else()
  target_link_libraries(catalyst_window_waiting PRIVATE catalyst_platform)
endif()

target_include_directories(catalyst_window_waiting PRIVATE ${PROJECT_SOURCE_DIR}/include)
