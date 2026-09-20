# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(
  granit_lighting_pbr_render_test
  integration/pbr_render_test.cpp
  "${PROJECT_SOURCE_DIR}/tests/support/pbr_test_support.cpp"
  "${PROJECT_SOURCE_DIR}/tests/support/reference/lighting/ibl_reference.cpp"
  "${PROJECT_SOURCE_DIR}/tests/support/reference/lighting/lighting_reference.cpp"
  "${PROJECT_SOURCE_DIR}/tests/support/reference/lighting/tone_mapping_reference.cpp"
  "${PROJECT_SOURCE_DIR}/tests/support/reference/material/pbr_reference.cpp"
)
add_dependencies(granit_lighting_pbr_render_test granit_test_shader_assets)
target_include_directories(granit_lighting_pbr_render_test PRIVATE "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(granit_lighting_pbr_render_test PRIVATE granit::lighting)
target_compile_definitions(
  granit_lighting_pbr_render_test
  PRIVATE
    GRANIT_PBR_SHADER_DIR="${GRANIT_PBR_TEST_SHADER_DIR}"
    GRANIT_PIPELINE_SHADER_DIR="${GRANIT_PIPELINE_TEST_SHADER_DIR}"
)
granit_target_compile_warnings(granit_lighting_pbr_render_test)
granit_target_output_directories(granit_lighting_pbr_render_test)
add_test(NAME granit.lighting.pbr_render COMMAND granit_lighting_pbr_render_test)
set_property(TEST granit.lighting.pbr_render APPEND PROPERTY LABELS gpu integration)

add_executable(
  granit_lighting_test
  ../support/reference/lighting/ibl_reference.cpp
  ../support/reference/lighting/lighting_reference.cpp
  ../support/reference/lighting/shadow_reference.cpp
  ../support/reference/lighting/tone_mapping_reference.cpp
  ../support/reference/material/pbr_reference.cpp
  directional_shadow_test.cpp
  ibl_reference_test.cpp
  ibl_resources_test.cpp
  light_buffers_test.cpp
  light_data_test.cpp
  lighting_reference_test.cpp
  multi_view_gpu_test.cpp
  forward_pipeline_graph_test.cpp
  shadow_resources_test.cpp
  shadow_ibl_resources_test.cpp
  shadow_reference_test.cpp
  tone_mapping_reference_test.cpp
  tone_mapping_pass_test.cpp
  tone_mapping_resources_test.cpp
  "${PROJECT_SOURCE_DIR}/tests/support/pbr_test_support.cpp"
)
target_include_directories(
  granit_lighting_test
  PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/tests"
)
target_link_libraries(granit_lighting_test PRIVATE granit::lighting Catch2::Catch2WithMain)
target_compile_options(granit_lighting_test PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>)
target_compile_definitions(
  granit_lighting_test
  PRIVATE
    GRANIT_PBR_SHADER_DIR="${GRANIT_PBR_TEST_SHADER_DIR}"
    GRANIT_PIPELINE_SHADER_DIR="${GRANIT_PIPELINE_TEST_SHADER_DIR}"
)
add_dependencies(granit_lighting_test granit_test_shader_assets)
granit_target_compile_warnings(granit_lighting_test)
granit_target_output_directories(granit_lighting_test)
add_test(NAME granit.lighting.resources COMMAND granit_lighting_test)

set_property(TEST granit.lighting.resources APPEND PROPERTY LABELS gpu integration)
