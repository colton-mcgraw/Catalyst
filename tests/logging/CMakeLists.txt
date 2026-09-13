foreach(suite level router sinks)
  add_executable(catalyst_tests_logging_${suite}
    test_${suite}.cpp
  )

  target_link_libraries(catalyst_tests_logging_${suite}
    PRIVATE
      catalyst::logging
  )

  add_test(
    NAME catalyst.logging.${suite}
    COMMAND catalyst_tests_logging_${suite}
  )
endforeach()
