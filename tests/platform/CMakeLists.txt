add_executable(catalyst_tests_platform_window
	test_window.cpp
)

target_link_libraries(catalyst_tests_platform_window
	PRIVATE
		catalyst::platform
		catalyst::math
)

add_test(
	NAME catalyst.platform.window
	COMMAND catalyst_tests_platform_window
)
