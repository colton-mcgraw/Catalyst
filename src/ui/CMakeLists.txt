add_library(catalyst_ui STATIC
  ui.cpp
)

add_library(catalyst::ui ALIAS catalyst_ui)

target_include_directories(catalyst_ui
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
