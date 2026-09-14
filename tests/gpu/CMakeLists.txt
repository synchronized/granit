# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set_property(DIRECTORY APPEND PROPERTY LABELS gpu integration)

function(granit_add_gpu_test_target target source)
  add_executable(${target} ${source})
  add_dependencies(${target} granit_test_shader_assets)
  granit_target_compile_warnings(${target})
  granit_target_output_directories(${target})
endfunction()

granit_add_gpu_test_target(granit_gpu_compute_test compute_test.cpp)
target_link_libraries(granit_gpu_compute_test PRIVATE granit::granit)
target_include_directories(granit_gpu_compute_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(granit_gpu_compute_test PRIVATE $<TARGET_OBJECTS:granit_internal_shader_format>)
target_compile_definitions(
  granit_gpu_compute_test PRIVATE GRANIT_SMOKE_ASSET_DIR="${GRANIT_SMOKE_TEST_SHADER_DIR}"
)
add_test(NAME granit.gpu.compute COMMAND granit_gpu_compute_test)

granit_add_gpu_test_target(granit_gpu_pbr_render_test pbr_render_test.cpp)
target_sources(
  granit_gpu_pbr_render_test
  PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/support/pbr_test_support.cpp"
    "${PROJECT_SOURCE_DIR}/tests/reference/lighting/ibl_reference.cpp"
    "${PROJECT_SOURCE_DIR}/tests/reference/lighting/lighting_reference.cpp"
    "${PROJECT_SOURCE_DIR}/tests/reference/lighting/tone_mapping_reference.cpp"
    "${PROJECT_SOURCE_DIR}/tests/reference/material/pbr_reference.cpp"
)
target_include_directories(granit_gpu_pbr_render_test PRIVATE "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(granit_gpu_pbr_render_test PRIVATE granit::lighting)
target_compile_definitions(
  granit_gpu_pbr_render_test
  PRIVATE
    GRANIT_PBR_SHADER_DIR="${GRANIT_PBR_TEST_SHADER_DIR}"
    GRANIT_PIPELINE_SHADER_DIR="${GRANIT_PIPELINE_TEST_SHADER_DIR}"
)
add_test(NAME granit.gpu.pbr_render COMMAND granit_gpu_pbr_render_test)

granit_add_gpu_test_target(granit_gpu_render_pipeline_test render_pipeline_test.cpp)
target_link_libraries(granit_gpu_render_pipeline_test PRIVATE granit::render_pipeline)
target_compile_definitions(
  granit_gpu_render_pipeline_test
  PRIVATE GRANIT_RENDER_PIPELINE_SMOKE_PACKAGE="${granit_render_pipeline_test_package}"
          GRANIT_PIPELINE_ASSET_DIR="${GRANIT_PBR_TEST_SHADER_DIR}"
)
target_include_directories(
  granit_gpu_render_pipeline_test PRIVATE "${PROJECT_SOURCE_DIR}/tests" "${PROJECT_SOURCE_DIR}/src"
)
add_dependencies(
  granit_gpu_render_pipeline_test granit_asset_tool granit_render_pipeline_test_package
)
target_sources(
  granit_gpu_render_pipeline_test
  PRIVATE "${granit_render_pipeline_test_package}"
          $<TARGET_OBJECTS:granit_internal_shader_format>
)
add_test(NAME granit.gpu.render_pipeline COMMAND granit_gpu_render_pipeline_test)

if(WIN32)
  granit_add_gpu_test_target(granit_gpu_hdr_window_test hdr_window_test.cpp)
  target_sources(
    granit_gpu_hdr_window_test PRIVATE "${PROJECT_SOURCE_DIR}/tests/support/pbr_test_support.cpp"
  )
  target_include_directories(granit_gpu_hdr_window_test PRIVATE "${PROJECT_SOURCE_DIR}/tests")
  target_link_libraries(granit_gpu_hdr_window_test PRIVATE granit::lighting user32)
  target_compile_definitions(
    granit_gpu_hdr_window_test
    PRIVATE
      GRANIT_PBR_SHADER_DIR="${GRANIT_PBR_TEST_SHADER_DIR}"
      GRANIT_PIPELINE_SHADER_DIR="${GRANIT_PIPELINE_TEST_SHADER_DIR}"
  )
  add_test(NAME granit.gpu.hdr_window COMMAND granit_gpu_hdr_window_test --smoke-test)
  set_tests_properties(granit.gpu.hdr_window PROPERTIES TIMEOUT 30)
  set_property(TEST granit.gpu.hdr_window APPEND PROPERTY LABELS platform)
endif()
