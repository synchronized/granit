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

add_executable(granit_compute_example compute.cpp)
target_link_libraries(granit_compute_example PRIVATE granit::granit)
target_compile_definitions(
  granit_compute_example
  PRIVATE GRANIT_EXAMPLE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
granit_target_compile_warnings(granit_compute_example)

add_executable(granit_material_hot_reload_example material_hot_reload.cpp)
target_link_libraries(granit_material_hot_reload_example PRIVATE granit::material)
target_compile_definitions(
  granit_material_hot_reload_example
  PRIVATE GRANIT_EXAMPLE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
granit_target_compile_warnings(granit_material_hot_reload_example)

add_executable(granit_pbr_offscreen_example pbr_offscreen.cpp)
target_link_libraries(granit_pbr_offscreen_example PRIVATE granit::lighting)
target_compile_definitions(
  granit_pbr_offscreen_example
  PRIVATE GRANIT_EXAMPLE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
granit_target_compile_warnings(granit_pbr_offscreen_example)
if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
  add_test(NAME granit.example.pbr_offscreen COMMAND granit_pbr_offscreen_example)
endif()

if(WIN32)
  add_executable(granit_window_clear_example WIN32 window_clear.cpp)
  target_link_libraries(granit_window_clear_example PRIVATE granit::granit user32)
  granit_target_compile_warnings(granit_window_clear_example)

  add_executable(granit_window_triangle_example window_triangle.cpp)
  target_link_libraries(granit_window_triangle_example PRIVATE granit::granit user32)
  target_compile_definitions(
    granit_window_triangle_example
    PRIVATE GRANIT_EXAMPLE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
  )
  granit_target_compile_warnings(granit_window_triangle_example)
endif()

set(
  granit_example_targets
  granit_version_example
  granit_renderer_example
  granit_offscreen_clear_example
  granit_offscreen_triangle_example
  granit_compute_example
  granit_material_hot_reload_example
  granit_pbr_offscreen_example
)
if(WIN32)
  list(APPEND granit_example_targets granit_window_clear_example granit_window_triangle_example)
endif()
foreach(target IN LISTS granit_example_targets)
  granit_target_output_directories(${target})
endforeach()
