add_executable(catalyst_window_polling
  main.cpp
)

# Prefer the monolithic library if enabled; otherwise link the platform module.
if(TARGET catalyst)
  target_link_libraries(catalyst_window_polling PRIVATE catalyst)
else()
  target_link_libraries(catalyst_window_polling PRIVATE catalyst_platform)
endif()

target_include_directories(catalyst_window_polling PRIVATE ${PROJECT_SOURCE_DIR}/include)
