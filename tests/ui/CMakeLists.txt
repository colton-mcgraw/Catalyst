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
