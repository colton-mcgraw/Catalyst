foreach(suite world transform bounds extract)
	add_executable(catalyst_tests_scene_${suite}
		test_${suite}.cpp
	)

	target_link_libraries(catalyst_tests_scene_${suite}
		PRIVATE
			catalyst::scene
	)

	add_test(
		NAME catalyst.scene.${suite}
		COMMAND catalyst_tests_scene_${suite}
	)
endforeach()
