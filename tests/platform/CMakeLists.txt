# catalyst::platform used to be exercised only through the examples and the modules built on it.
# These suites drive the public window and monitor API directly.
#
# They run against the null backend only. The assertions are about the module's contract -- a
# created window announces its geometry, destroying one publishes window_destroyed_event, size
# limits clamp, wait_events() returns at once when nothing can wake it -- and the null backend is
# the one implementation of that contract that behaves identically on every machine. Against
# Win32 the same calls would create real windows on the test runner's desktop, and several of the
# answers (monitor geometry, DPI, what minimising a hidden window does) belong to the machine, not
# to Catalyst. Configure with -DCATALYST_PLATFORM_BACKEND=null to run them on Windows; CI's Linux
# jobs run them as a matter of course.

if(NOT TARGET catalyst_platform_backend_null)
	message(STATUS "Catalyst tests 'platform' skipped: they need CATALYST_PLATFORM_BACKEND=null")
	return()
endif()

foreach(_test window monitor)
	add_executable(catalyst_tests_platform_${_test}
		test_${_test}.cpp
	)

	target_link_libraries(catalyst_tests_platform_${_test}
		PRIVATE
			catalyst::platform
			catalyst::events
	)

	add_test(
		NAME catalyst.platform.${_test}
		COMMAND catalyst_tests_platform_${_test}
	)
endforeach()
