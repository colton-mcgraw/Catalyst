add_executable(catalyst_tests_input_keyboard
	test_keyboard.cpp
)

target_link_libraries(catalyst_tests_input_keyboard
	PRIVATE
		catalyst::input
		catalyst::core
)

add_test(
	NAME catalyst.input.keyboard
	COMMAND catalyst_tests_input_keyboard
)

add_executable(catalyst_tests_input_state
	test_state.cpp
)

target_link_libraries(catalyst_tests_input_state
	PRIVATE
		catalyst::input
		catalyst::core
)

add_test(
	NAME catalyst.input.state
	COMMAND catalyst_tests_input_state
)

add_executable(catalyst_tests_input_gamepad
	test_gamepad.cpp
)

target_link_libraries(catalyst_tests_input_gamepad
	PRIVATE
		catalyst::input
		catalyst::core
)

add_test(
	NAME catalyst.input.gamepad
	COMMAND catalyst_tests_input_gamepad
)
