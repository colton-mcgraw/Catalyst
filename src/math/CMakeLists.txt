# catalyst::math is header-only, so this is an INTERFACE library: it carries the include directory
# and nothing else. It used to be a STATIC library whose one translation unit defined a
# `module_name()` that no public header declared -- a symbol nothing could call. That file is gone.
add_library(catalyst_math INTERFACE)

add_library(catalyst::math ALIAS catalyst_math)

target_include_directories(catalyst_math
  INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# The generated <catalyst/config.hpp>.
target_link_libraries(catalyst_math INTERFACE catalyst::config)
