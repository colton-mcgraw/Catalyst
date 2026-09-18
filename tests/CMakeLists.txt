# One suite per module, in tests/<module>/, added when that module is built. The module switches
# are an advertised feature, and a test tree that ignored them turned any subset build into a link
# error -- which is how -DCATALYST_BUILD_AUDIO=OFF used to fail: not at configure time with a clear
# message, but much later, building a test for a module that was not there.
#
# The list of modules is the manifest's (cmake/CatalystModules.cmake), so a new module's suite is
# picked up by creating the directory. A module without a suite is simply skipped; a sparse checkout
# that left a suite out is skipped the same way.
#
# tests/consumer/ is not a suite. It is a standalone project that consumes an *installed* Catalyst
# through find_package(), which CI drives; see the comment at the top of its CMakeLists.txt.

foreach(_module IN LISTS CATALYST_MODULES)
	if(${CATALYST_MODULE_${_module}_OPTION} AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_module}/CMakeLists.txt")
		add_subdirectory(${_module})
	endif()
endforeach()
