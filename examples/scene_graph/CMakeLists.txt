# CMakeLists.txt for the Catalyst scene_graph example.
# Builds a world of entities with a transform hierarchy, attaches camera, light, renderable and an
# application component, then runs the per-frame scene flow -- update_transforms, make_view, extract
# -- and logs the resulting render list. No window and no device: the scene module produces a draw
# list rather than drawing, so the whole of a frame's scene work runs in a console program.
# License: MIT (see LICENSE).

add_executable(catalyst_scene_graph
  main.cpp
)

if(TARGET catalyst)
  target_link_libraries(catalyst_scene_graph PRIVATE catalyst)
else()
  target_link_libraries(catalyst_scene_graph PRIVATE catalyst::scene catalyst::logging)
endif()

target_include_directories(catalyst_scene_graph PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(catalyst_scene_graph PRIVATE catalyst::config)
