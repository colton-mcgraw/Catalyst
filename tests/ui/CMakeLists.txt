foreach(suite measurement geometry tree layout batch paint hit_test interaction text)
	add_executable(catalyst_tests_ui_${suite}
		test_${suite}.cpp
	)

	target_link_libraries(catalyst_tests_ui_${suite}
		PRIVATE
			catalyst::ui
	)

	add_test(
		NAME catalyst.ui.${suite}
		COMMAND catalyst_tests_ui_${suite}
	)
endforeach()

# The renderer bridge is its own target and needs a device, like the rendering tests do. It takes the
# same CATALYST_RENDERING_VALIDATION switch (tests/rendering/validation_option.hpp), hence logging.
if(TARGET catalyst::ui_renderer AND TARGET catalyst::logging)
	add_executable(catalyst_tests_ui_renderer
		test_renderer.cpp
	)

	target_link_libraries(catalyst_tests_ui_renderer
		PRIVATE
			catalyst::ui_renderer
			catalyst::logging
	)

	add_test(
		NAME catalyst.ui.renderer
		COMMAND catalyst_tests_ui_renderer
	)
endif()

# The shell: the input bridge is headless, the host needs a device like the renderer bridge does.
if(TARGET catalyst::ui_host AND TARGET catalyst::logging)
	add_executable(catalyst_tests_ui_input_bridge
		test_input_bridge.cpp
	)
	target_link_libraries(catalyst_tests_ui_input_bridge
		PRIVATE
			catalyst::ui_host
	)
	add_test(
		NAME catalyst.ui.input_bridge
		COMMAND catalyst_tests_ui_input_bridge
	)

	add_executable(catalyst_tests_ui_host
		test_host.cpp
	)
	target_link_libraries(catalyst_tests_ui_host
		PRIVATE
			catalyst::ui_host
			catalyst::logging
	)
	add_test(
		NAME catalyst.ui.host
		COMMAND catalyst_tests_ui_host
	)
endif()
