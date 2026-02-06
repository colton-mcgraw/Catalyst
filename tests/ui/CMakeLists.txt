add_executable(catalyst_tests_ui_measurement
	test_measurement.cpp
)

target_link_libraries(catalyst_tests_ui_measurement
	PRIVATE
		catalyst::ui
)

add_test(
	NAME catalyst.ui.measurement
	COMMAND catalyst_tests_ui_measurement
)
