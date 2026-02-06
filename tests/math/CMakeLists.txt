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

add_executable(catalyst_tests_math_simd_ints
	test_simd_ints.cpp
)

target_link_libraries(catalyst_tests_math_simd_ints
	PRIVATE
		catalyst::math
)

add_test(
	NAME catalyst.math.simd.ints
	COMMAND catalyst_tests_math_simd_ints
)

add_executable(catalyst_tests_math_simd_doubles
	test_simd_doubles.cpp
)

target_link_libraries(catalyst_tests_math_simd_doubles
	PRIVATE
		catalyst::math
)

add_test(
	NAME catalyst.math.simd.doubles
	COMMAND catalyst_tests_math_simd_doubles
)

add_executable(catalyst_tests_math_simd_i64
	test_simd_i64.cpp
)

target_link_libraries(catalyst_tests_math_simd_i64
	PRIVATE
		catalyst::math
)

add_test(
	NAME catalyst.math.simd.i64
	COMMAND catalyst_tests_math_simd_i64
)

add_executable(catalyst_tests_math_simd_cmp_select
	test_simd_cmp_select.cpp
)

target_link_libraries(catalyst_tests_math_simd_cmp_select
	PRIVATE
		catalyst::math
)

add_test(
	NAME catalyst.math.simd.cmp_select
	COMMAND catalyst_tests_math_simd_cmp_select
)

add_executable(catalyst_tests_math_vec
	test_vec.cpp
)

target_link_libraries(catalyst_tests_math_vec
	PRIVATE
		catalyst::math
)

add_test(
	NAME catalyst.math.vec
	COMMAND catalyst_tests_math_vec
)

add_executable(catalyst_tests_math_mat
	test_mat.cpp
)

target_link_libraries(catalyst_tests_math_mat
	PRIVATE
		catalyst::math
)

add_test(
	NAME catalyst.math.mat
	COMMAND catalyst_tests_math_mat
)

add_executable(catalyst_tests_math_quat
	test_quat.cpp
)

target_link_libraries(catalyst_tests_math_quat
	PRIVATE
		catalyst::math
)

add_test(
	NAME catalyst.math.quat
	COMMAND catalyst_tests_math_quat
)

add_executable(catalyst_tests_math_euler
	test_euler.cpp
)

target_link_libraries(catalyst_tests_math_euler
	PRIVATE
		catalyst::math
)

add_test(
	NAME catalyst.math.euler
	COMMAND catalyst_tests_math_euler
)

add_executable(catalyst_tests_math_projection
	test_projection.cpp
)

target_link_libraries(catalyst_tests_math_projection
	PRIVATE
		catalyst::math
)

add_test(
	NAME catalyst.math.projection
	COMMAND catalyst_tests_math_projection
)

add_executable(catalyst_tests_math_transform
	test_transform.cpp
)

target_link_libraries(catalyst_tests_math_transform
	PRIVATE
		catalyst::math
)

add_test(
	NAME catalyst.math.transform
	COMMAND catalyst_tests_math_transform
)

add_executable(catalyst_tests_math_view
	test_view.cpp
)

target_link_libraries(catalyst_tests_math_view
	PRIVATE
		catalyst::math
)

add_test(
	NAME catalyst.math.view
	COMMAND catalyst_tests_math_view
)

add_executable(catalyst_tests_math_rect
	test_rect.cpp
)

target_link_libraries(catalyst_tests_math_rect
	PRIVATE
		catalyst::math
)

add_test(
	NAME catalyst.math.rect
	COMMAND catalyst_tests_math_rect
)
