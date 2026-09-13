# Geometry and layout are expressed with the vector and rectangle types from catalyst::math.
# Required, not optional: see cmake/CatalystModuleDeps.cmake.
include(CatalystModuleDeps)
catalyst_require_modules(ui math)

add_library(catalyst_ui STATIC
  ui.cpp
  tree.cpp
  layout.cpp
)

add_library(catalyst::ui ALIAS catalyst_ui)

target_include_directories(catalyst_ui
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# The generated <catalyst/config.hpp>.
target_link_libraries(catalyst_ui PUBLIC catalyst::config)

# Geometry and layout are expressed with the vector and rectangle types from catalyst::math.
target_link_libraries(catalyst_ui PUBLIC catalyst::math)
