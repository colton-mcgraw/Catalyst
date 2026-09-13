add_library(catalyst_scene STATIC
  scene.cpp
)

add_library(catalyst::scene ALIAS catalyst_scene)

target_include_directories(catalyst_scene
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# The generated <catalyst/config.hpp>.
target_link_libraries(catalyst_scene PUBLIC catalyst::config)
