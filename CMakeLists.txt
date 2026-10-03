cmake_minimum_required(VERSION 3.23)

project(Catalyst VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Fail here, with one sentence, rather than several hundred lines of template errors partway
# through the build. See cmake/CatalystCompilerSupport.cmake for what is checked and why.
include(CatalystCompilerSupport)
catalyst_require_cxx23_support()

# When using MSVC-style debug info (/Zi) with parallel builds, multiple compiler
# processes can contend for the shared PDB and appear to hang. /FS forces
# serialized PDB writes.
if(MSVC)
	add_compile_options(/FS)
endif()

# Contributor defaults versus consumer defaults. A checkout configured directly wants the examples,
# tests and benchmarks; a project that pulled Catalyst in with add_subdirectory() or FetchContent
# wants the library and nothing else, and should not have to know three switches to get it.
option(CATALYST_DEVELOPER_MODE
  "Default the examples, tests and benchmarks to ON. ON when this is the top-level project, OFF when Catalyst is a dependency of another one."
  ${PROJECT_IS_TOP_LEVEL})

option(CATALYST_BUILD_ALL "Build monolithic Catalyst library (links enabled modules)" ON)

# The modules, one declaration each, in cmake/CatalystModules.cmake. This turns every declaration
# into a CATALYST_BUILD_<MODULE> switch. Implemented modules default ON; the placeholders
# (animation, net, physics, utils) default OFF; and any module whose src/ directory is not in the
# checkout defaults OFF, which is what lets a sparse checkout of a few modules configure as is.
include(CatalystModules)
catalyst_module_options()

# Image decoding in catalyst::resource. ON fetches stb_image at a pinned commit (cmake/stb.cmake),
# which needs network access the first time a build tree is configured. OFF keeps the resource
# module dependency-free and makes load_image report unsupported_format instead of decoding.
option(CATALYST_RESOURCE_STB "Fetch stb_image and build the resource module's image decoders" ON)

# Each extra follows developer mode, and each also needs its directory to be present: a sparse
# checkout that took only the library should not fail over a tests/ that was never fetched.
foreach(_extra "EXAMPLES examples" "TESTS tests" "BENCHMARKS bench")
	separate_arguments(_extra)
	list(GET _extra 0 _option)
	list(GET _extra 1 _dir)

	if(CATALYST_DEVELOPER_MODE AND EXISTS "${PROJECT_SOURCE_DIR}/${_dir}/CMakeLists.txt")
		set(_default ON)
	else()
		set(_default OFF)
	endif()

	option(CATALYST_BUILD_${_option} "Build Catalyst ${_dir} (defaults to CATALYST_DEVELOPER_MODE)" ${_default})

	if(CATALYST_BUILD_${_option} AND NOT EXISTS "${PROJECT_SOURCE_DIR}/${_dir}/CMakeLists.txt")
		message(FATAL_ERROR
			"CATALYST_BUILD_${_option}=ON, but ${_dir}/ is not in this checkout.\n"
			"If this is a sparse checkout: git sparse-checkout add ${_dir}\n"
			"Otherwise: -DCATALYST_BUILD_${_option}=OFF\n")
	endif()
endforeach()
unset(_extra)
unset(_option)
unset(_dir)
unset(_default)

add_subdirectory(src)

if(CATALYST_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

if(CATALYST_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()

if(CATALYST_BUILD_BENCHMARKS)
	add_subdirectory(bench)
endif()

# API reference. The Doxyfile is generated from Doxyfile.in so that project() above is the only
# place the version number is written; `cmake --build <tree> --target docs` puts the HTML in
# <tree>/docs/html, which is also what the docs workflow publishes to GitHub Pages.
if(CATALYST_DEVELOPER_MODE)
	configure_file(Doxyfile.in "${PROJECT_BINARY_DIR}/Doxyfile" @ONLY)

	find_package(Doxygen QUIET)
	if(DOXYGEN_FOUND)
		add_custom_target(docs
			COMMAND Doxygen::doxygen "${PROJECT_BINARY_DIR}/Doxyfile"
			WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
			COMMENT "Generating the API reference with Doxygen"
			VERBATIM)
	endif()
endif()
