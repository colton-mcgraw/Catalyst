add_library(catalyst_physics STATIC
  physics.cpp
)

add_library(catalyst::physics ALIAS catalyst_physics)

target_include_directories(catalyst_physics
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
