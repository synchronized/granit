# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(granit_version_example version.cpp)
target_link_libraries(granit_version_example PRIVATE granit::granit)
granit_target_compile_warnings(granit_version_example)

add_executable(granit_renderer_example renderer.cpp)
target_link_libraries(granit_renderer_example PRIVATE granit::granit)
granit_target_compile_warnings(granit_renderer_example)

add_executable(granit_offscreen_clear_example offscreen_clear.cpp)
target_link_libraries(granit_offscreen_clear_example PRIVATE granit::granit)
granit_target_compile_warnings(granit_offscreen_clear_example)

add_executable(granit_offscreen_triangle_example offscreen_triangle.cpp)
target_link_libraries(granit_offscreen_triangle_example PRIVATE granit::granit)
target_compile_definitions(
  granit_offscreen_triangle_example
  PRIVATE GRANIT_EXAMPLE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
granit_target_compile_warnings(granit_offscreen_triangle_example)

if(WIN32)
  add_executable(granit_window_clear_example WIN32 window_clear.cpp)
  target_link_libraries(granit_window_clear_example PRIVATE granit::granit user32)
  granit_target_compile_warnings(granit_window_clear_example)
endif()
