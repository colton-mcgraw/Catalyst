# A project that consumes an *installed* Catalyst through find_package(). Not a test suite: nothing
# here is added by tests/CMakeLists.txt, and it must never see the source tree's targets. It exists
# because the install and the package config are part of the public surface, and something has to
# prove that a separate project can actually consume them. It used to be a shell heredoc in CI;
# as a committed project it can be run locally, reviewed, and grown.
#
# What it checks:
#   - find_package(Catalyst COMPONENTS ...) finds the requested modules and their targets;
#   - a component the install does not contain is reported absent, not silently missing;
#   - the umbrella <catalyst/catalyst.hpp> compiles against the installed headers alone;
#   - catalyst::catalyst, when it was built, links and runs.
#
# To run it locally, install Catalyst somewhere and point this project at it:
#
#   cmake -S . -B build-install -DCMAKE_INSTALL_PREFIX=$PWD/install-prefix
#   cmake --build build-install --parallel
#   cmake --install build-install
#   cmake -S tests/consumer -B build-consumer -DCMAKE_PREFIX_PATH=$PWD/install-prefix
#   cmake --build build-consumer
#   ctest --test-dir build-consumer --output-on-failure
#
# Knobs:
#   CATALYST_CONSUMER_COMPONENTS     the components to require (default: audio;rendering;math;events)
#   CATALYST_CONSUMER_VERSION        a version to require, in find_package's terms (default: any)
#   CATALYST_CONSUMER_EXPECT_ABSENT  components the install must NOT have (default: net, which is
#                                    a placeholder and never installed)

cmake_minimum_required(VERSION 3.23)

project(CatalystConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CATALYST_CONSUMER_COMPONENTS "audio;rendering;math;events" CACHE STRING
    "Catalyst components this consumer requires")
set(CATALYST_CONSUMER_VERSION "" CACHE STRING
    "Catalyst version to require (empty: any)")
set(CATALYST_CONSUMER_EXPECT_ABSENT "net" CACHE STRING
    "Catalyst components the install must not contain (empty: skip the check)")

find_package(Catalyst ${CATALYST_CONSUMER_VERSION} REQUIRED COMPONENTS ${CATALYST_CONSUMER_COMPONENTS})

message(STATUS "Catalyst ${Catalyst_VERSION} from ${Catalyst_DIR}")
message(STATUS "  modules:           ${CATALYST_MODULES}")
message(STATUS "  rendering backend: ${CATALYST_RENDERING_BACKEND_NAME}")

foreach(_component IN LISTS CATALYST_CONSUMER_COMPONENTS)
  if(NOT Catalyst_${_component}_FOUND OR NOT TARGET catalyst::${_component})
    message(FATAL_ERROR "find_package succeeded but component '${_component}' has no target")
  endif()
endforeach()

# The package config marks a missing optional component as not found rather than failing the
# whole call, and a REQUIRED one fails with a message naming what the install does contain. The
# first is checked here; CI checks the second by asking for a placeholder as REQUIRED and
# expecting the configure to fail.
if(CATALYST_CONSUMER_EXPECT_ABSENT)
  find_package(Catalyst OPTIONAL_COMPONENTS ${CATALYST_CONSUMER_EXPECT_ABSENT})
  foreach(_component IN LISTS CATALYST_CONSUMER_EXPECT_ABSENT)
    if(Catalyst_${_component}_FOUND OR TARGET catalyst::${_component})
      message(FATAL_ERROR
        "Component '${_component}' was expected to be absent from this install, but it is there. "
        "Installed modules: ${CATALYST_MODULES}")
    endif()
  endforeach()
endif()

add_executable(consumer main.cpp)

foreach(_component IN LISTS CATALYST_CONSUMER_COMPONENTS)
  target_link_libraries(consumer PRIVATE catalyst::${_component})
endforeach()

# The monolithic library is optional in an install (CATALYST_BUILD_ALL). When it is there, link it
# too: it is the target most consumers reach for first, and it is how catalyst::version() below
# gets resolved without the fixture having to know which module carries it.
if(TARGET catalyst::catalyst)
  target_link_libraries(consumer PRIVATE catalyst::catalyst)
  target_compile_definitions(consumer PRIVATE CATALYST_CONSUMER_HAS_MONOLITH=1)
  message(STATUS "  catalyst::catalyst: linked")
else()
  message(STATUS "  catalyst::catalyst: not in this install")
endif()

enable_testing()
add_test(NAME catalyst.consumer COMMAND consumer)
