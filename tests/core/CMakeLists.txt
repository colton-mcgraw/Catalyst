add_executable(catalyst_tests_core_dispatcher
	test_dispatcher.cpp
)

target_link_libraries(catalyst_tests_core_dispatcher
	PRIVATE
		catalyst::core
)

find_package(Threads REQUIRED)
target_link_libraries(catalyst_tests_core_dispatcher PRIVATE Threads::Threads)

add_test(
	NAME catalyst.core.dispatcher
	COMMAND catalyst_tests_core_dispatcher
)

add_executable(catalyst_tests_core_event_queue
	test_event_queue.cpp
)

target_link_libraries(catalyst_tests_core_event_queue
	PRIVATE
		catalyst::core
)

target_link_libraries(catalyst_tests_core_event_queue PRIVATE Threads::Threads)

add_test(
	NAME catalyst.core.event_queue
	COMMAND catalyst_tests_core_event_queue
)

add_executable(catalyst_tests_core_static_init
	test_static_init.cpp
	test_static_init_tu.cpp
)

target_link_libraries(catalyst_tests_core_static_init
	PRIVATE
		catalyst::core
)

add_test(
	NAME catalyst.core.static_init
	COMMAND catalyst_tests_core_static_init
)
