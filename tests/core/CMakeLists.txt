# catalyst::core is small -- the version string and move_only_function -- and used to be covered
# only through the modules that link it. That is no longer enough: the version string is what an
# installed consumer checks against CATALYST_VERSION_STRING, and move_only_function is what every
# module stores its callbacks in, so both get a suite of their own.

add_executable(catalyst_tests_core
	test_core.cpp
)

target_link_libraries(catalyst_tests_core
	PRIVATE
		catalyst::core
)

add_test(
	NAME catalyst.core
	COMMAND catalyst_tests_core
)
