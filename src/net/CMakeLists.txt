add_library(catalyst_net STATIC
  net.cpp
)

add_library(catalyst::net ALIAS catalyst_net)

target_include_directories(catalyst_net
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
