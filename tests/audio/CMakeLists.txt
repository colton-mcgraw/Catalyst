add_executable(catalyst_tests_audio_engine
	test_engine.cpp
)

target_link_libraries(catalyst_tests_audio_engine
	PRIVATE
		catalyst::audio
)

add_test(
	NAME catalyst.audio.engine
	COMMAND catalyst_tests_audio_engine
)

add_executable(catalyst_tests_audio_offline
	test_offline.cpp
)

target_link_libraries(catalyst_tests_audio_offline
	PRIVATE
		catalyst::audio
)

add_test(
	NAME catalyst.audio.offline
	COMMAND catalyst_tests_audio_offline
)
