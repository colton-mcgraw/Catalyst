add_library(catalyst_utils STATIC
  utils.cpp
)

add_library(catalyst::utils ALIAS catalyst_utils)

target_include_directories(catalyst_utils
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
