add_executable(catalyst_bench_events_dispatcher
  dispatcher.cpp
)

target_link_libraries(catalyst_bench_events_dispatcher
  PRIVATE
    catalyst::core
)

target_include_directories(catalyst_bench_events_dispatcher
  PRIVATE
    ${PROJECT_SOURCE_DIR}/bench
)