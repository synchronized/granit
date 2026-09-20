# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(
  granit_pipeline_test
  material_api_test.cpp
  pbr_material_api_test.cpp
  environment_map_api_test.cpp
  environment_asset_test.cpp
  $<TARGET_OBJECTS:granit_asset_format_environment>
  $<TARGET_OBJECTS:granit_asset_format_shader>
  mesh_api_test.cpp
  render_pipeline_api_test.cpp
  scene_api_test.cpp
  unlit_pass_test.cpp
  canvas_draw_list_test.cpp
  canvas_draw_list_api_test.cpp
  canvas_geometry_upload_test.cpp
  canvas_pass_test.cpp
  "${PROJECT_SOURCE_DIR}/src/pipeline/dynamic_uniform_arena.cpp"
  dynamic_uniform_arena_test.cpp
  debug_draw_list_api_test.cpp
  debug_draw_geometry_test.cpp
  text_draw_list_api_test.cpp
  text_atlas_api_test.cpp
)
set(granit_unlit_test_package "${CMAKE_CURRENT_BINARY_DIR}/assets/unlit.grmat")
add_custom_command(
  OUTPUT "${granit_unlit_test_package}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets"
  COMMAND granit_asset_tool material build "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke/unlit.grmat.json"
          --output "${granit_unlit_test_package}" --shader-index
          "${GRANIT_UNLIT_TEST_SHADER_INDEX}"
  DEPENDS
    granit_asset_tool
    granit_test_shader_assets
    "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke/unlit.grmat.json"
    "${GRANIT_UNLIT_TEST_SHADER_INDEX}"
  VERBATIM
)
add_custom_target(granit_unlit_test_package DEPENDS "${granit_unlit_test_package}")
add_dependencies(granit_pipeline_test granit_unlit_test_package)
target_include_directories(
  granit_pipeline_test PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/tests"
)
target_link_libraries(
  granit_pipeline_test
  PRIVATE
    granit::render_pipeline
    granit::lighting
    granit::material
    granit::granit
    Catch2::Catch2WithMain
)
target_compile_options(granit_pipeline_test PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>)
target_compile_definitions(
  granit_pipeline_test
  PRIVATE
    GRANIT_PIPELINE_ASSET_DIR="${GRANIT_PBR_TEST_SHADER_DIR}"
    GRANIT_SHADER_OBJECT_FIXTURE_DIR="${GRANIT_TEST_BINARY_DIR}/asset_tools/test-objects"
    GRANIT_TEST_ASSET_DIR="${granit_test_shader_fixture_dir}"
    GRANIT_PIPELINE_SHADER_DIR="${GRANIT_PIPELINE_TEST_SHADER_DIR}"
    GRANIT_UNLIT_TEST_PACKAGE="${granit_unlit_test_package}"
    GRANIT_UNLIT_SHADER_DIR="${GRANIT_UNLIT_TEST_SHADER_DIR}"
)
add_dependencies(
  granit_pipeline_test granit_shader_fixture_objects granit_test_shader_assets
)
granit_target_compile_warnings(granit_pipeline_test)
granit_target_output_directories(granit_pipeline_test)
add_test(NAME granit.pipeline COMMAND granit_pipeline_test)

set_property(TEST granit.pipeline APPEND PROPERTY LABELS gpu integration)
