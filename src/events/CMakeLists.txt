# The listener and middleware registries store callables in core::detail::move_only_function.
# Required, not optional: see cmake/CatalystModuleDeps.cmake.
include(CatalystModuleDeps)
catalyst_require_modules(events core)

# catalyst::events is header-only, and is the module the rest of Catalyst is built on: audio, input,
# rendering, resource, platform and ui all publish to an events::bus.
#
# It had no target at all until now. It worked only because every module put include/ on its search
# path, which meant the dependency existed in fact but appeared nowhere in the build graph -- and so
# would have appeared nowhere in the install export either. An INTERFACE library states it: no
# translation units, no link cost, and consumers that say what they depend on.
add_library(catalyst_events INTERFACE)

add_library(catalyst::events ALIAS catalyst_events)

target_include_directories(catalyst_events
  INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# The generated <catalyst/config.hpp>.
target_link_libraries(catalyst_events INTERFACE catalyst::config)

# The listener and middleware registries store callables in core::detail::move_only_function.
target_link_libraries(catalyst_events INTERFACE catalyst::core)
