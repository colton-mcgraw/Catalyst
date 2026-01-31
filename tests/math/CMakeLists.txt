add_executable(catalyst_tests_math_simd
	test_simd.cpp
)

target_link_libraries(catalyst_tests_math_simd
	PRIVATE
		catalyst::math
)

add_test(
	NAME catalyst.math.simd
	COMMAND catalyst_tests_math_simd
)

add_executable(catalyst_tests_math_simd_edge_cases
	test_simd_edge_cases.cpp
)

target_link_libraries(catalyst_tests_math_simd_edge_cases
	PRIVATE
		catalyst::math
)

add_test(
	NAME catalyst.math.simd.edge_cases
	COMMAND catalyst_tests_math_simd_edge_cases
)
