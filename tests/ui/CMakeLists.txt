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
