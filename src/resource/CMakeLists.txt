add_library(catalyst_resource STATIC
  resource.cpp
)

add_library(catalyst::resource ALIAS catalyst_resource)

target_include_directories(catalyst_resource
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
