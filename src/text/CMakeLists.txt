# catalyst::text is header-only: UTF-8 decoding and the scanner the JSON, CSV and OBJ parsers are
# built on. Like catalyst::events it had no target and no build switch; see src/events/CMakeLists.txt
# for why that mattered.
add_library(catalyst_text INTERFACE)

add_library(catalyst::text ALIAS catalyst_text)

target_include_directories(catalyst_text
  INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# The generated <catalyst/config.hpp>.
target_link_libraries(catalyst_text INTERFACE catalyst::config)
