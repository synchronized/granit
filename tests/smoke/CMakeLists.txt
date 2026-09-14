# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set_property(DIRECTORY APPEND PROPERTY LABELS smoke)

function(granit_add_smoke_target target source)
  add_executable(${target} ${source})
  add_dependencies(${target} granit_test_shader_assets)
  granit_target_compile_warnings(${target})
  granit_target_output_directories(${target})
endfunction()

granit_add_smoke_target(granit_gpu_offscreen_smoke gpu_offscreen.cpp)
target_link_libraries(granit_gpu_offscreen_smoke PRIVATE granit::granit)
target_include_directories(granit_gpu_offscreen_smoke PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(
  granit_gpu_offscreen_smoke PRIVATE $<TARGET_OBJECTS:granit_internal_shader_format>
)
target_compile_definitions(
  granit_gpu_offscreen_smoke
  PRIVATE GRANIT_SMOKE_ASSET_DIR="${GRANIT_SMOKE_TEST_SHADER_DIR}"
)
add_test(NAME granit.smoke.gpu_offscreen COMMAND granit_gpu_offscreen_smoke)

granit_add_smoke_target(granit_material_hot_reload_smoke material_hot_reload.cpp)
target_link_libraries(granit_material_hot_reload_smoke PRIVATE granit::material)
target_compile_definitions(
  granit_material_hot_reload_smoke
  PRIVATE GRANIT_SMOKE_ASSET_DIR="${GRANIT_SMOKE_TEST_SHADER_DIR}"
)
add_test(NAME granit.smoke.material_hot_reload COMMAND granit_material_hot_reload_smoke)

granit_add_smoke_target(granit_immediate_ui_adapter_smoke immediate_ui_adapter.cpp)
target_link_libraries(granit_immediate_ui_adapter_smoke PRIVATE granit::render_pipeline)
add_test(NAME granit.smoke.immediate_ui_adapter COMMAND granit_immediate_ui_adapter_smoke)

set_property(
  TEST
    granit.smoke.gpu_offscreen
    granit.smoke.material_hot_reload
    granit.smoke.immediate_ui_adapter
  APPEND PROPERTY LABELS gpu integration
)

if(WIN32)
  granit_add_smoke_target(granit_render_pipeline_window_smoke render_pipeline_window.cpp)
  target_link_libraries(granit_render_pipeline_window_smoke PRIVATE granit::render_pipeline user32)
  target_compile_definitions(
    granit_render_pipeline_window_smoke
    PRIVATE GRANIT_RENDER_PIPELINE_SMOKE_PACKAGE="${granit_render_pipeline_test_package}"
            GRANIT_PIPELINE_ASSET_DIR="${GRANIT_PBR_TEST_SHADER_DIR}"
  )
  target_include_directories(
    granit_render_pipeline_window_smoke PRIVATE "${PROJECT_SOURCE_DIR}/tests"
                                                "${PROJECT_SOURCE_DIR}/src"
  )
  add_dependencies(
    granit_render_pipeline_window_smoke granit_asset_tool granit_render_pipeline_test_package
  )
  target_sources(
    granit_render_pipeline_window_smoke
    PRIVATE "${granit_render_pipeline_test_package}"
            $<TARGET_OBJECTS:granit_internal_shader_format>
  )
  add_test(
    NAME granit.smoke.render_pipeline_window
    COMMAND granit_render_pipeline_window_smoke --smoke-test
  )
  set_tests_properties(
    granit.smoke.render_pipeline_window
    PROPERTIES TIMEOUT 30 FAIL_REGULAR_EXPRESSION "\\[granit\\]\\[vulkan\\]"
               LABELS "smoke;gpu;integration;platform"
  )

  granit_add_smoke_target(granit_window_triangle_smoke window_triangle.cpp)
  target_link_libraries(granit_window_triangle_smoke PRIVATE granit::granit user32)
  target_include_directories(granit_window_triangle_smoke PRIVATE "${PROJECT_SOURCE_DIR}/src")
  target_sources(
    granit_window_triangle_smoke PRIVATE $<TARGET_OBJECTS:granit_internal_shader_format>
  )
  target_compile_definitions(
    granit_window_triangle_smoke
    PRIVATE GRANIT_SMOKE_ASSET_DIR="${GRANIT_SMOKE_TEST_SHADER_DIR}"
  )
  add_test(NAME granit.smoke.window_triangle COMMAND granit_window_triangle_smoke --smoke-test)
  set_tests_properties(granit.smoke.window_triangle PROPERTIES TIMEOUT 30)

  set_property(TEST granit.smoke.window_triangle APPEND PROPERTY LABELS gpu integration platform)
endif()
