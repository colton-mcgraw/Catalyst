add_library(catalyst_animation STATIC
  animation.cpp
)

add_library(catalyst::animation ALIAS catalyst_animation)

target_include_directories(catalyst_animation
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
