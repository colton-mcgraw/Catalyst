add_executable(catalyst_tests_rendering_resources
	test_resources.cpp
)

target_link_libraries(catalyst_tests_rendering_resources
	PRIVATE
		catalyst::rendering
)

add_test(
	NAME catalyst.rendering.resources
	COMMAND catalyst_tests_rendering_resources
)
