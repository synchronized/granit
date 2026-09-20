# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(granit_pbr_test pbr_render_graph_adapter_test.cpp)
target_include_directories(granit_pbr_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(granit_pbr_test PRIVATE granit::pbr Catch2::Catch2WithMain)
target_compile_options(granit_pbr_test PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>)
granit_target_compile_warnings(granit_pbr_test)
granit_target_output_directories(granit_pbr_test)
add_test(NAME granit.pbr COMMAND granit_pbr_test)

add_executable(
  granit_material_test
  ../support/reference/material/pbr_reference.cpp
  material_gpu_instance_test.cpp
  material_hot_reload_test.cpp
  material_archive_test.cpp
  material_metadata_test.cpp
  material_migration_test.cpp
  material_package_test.cpp
  material_package_archive_test.cpp
  pbr_material_schema_test.cpp
  pbr_draw_inputs_test.cpp
  pbr_reference_test.cpp
  public_pbr_asset_test.cpp
  material_source_json_test.cpp
  material_template_gpu_test.cpp
  "${PROJECT_SOURCE_DIR}/src/asset_tools/material/debug_json.cpp"
  "${PROJECT_SOURCE_DIR}/src/asset_tools/material/source_json.cpp"
)
target_include_directories(
  granit_material_test PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/tests"
)
target_link_libraries(granit_material_test PRIVATE granit::material Catch2::Catch2WithMain)
target_sources(granit_material_test PRIVATE $<TARGET_OBJECTS:granit_asset_format_shader>)
target_compile_options(
  granit_material_test
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
target_compile_definitions(
  granit_material_test
  PRIVATE
    GRANIT_SHADER_OBJECT_FIXTURE_DIR="${GRANIT_TEST_BINARY_DIR}/asset_tools/test-objects"
    GRANIT_TEST_ASSET_DIR="${granit_test_shader_fixture_dir}"
    GRANIT_SMOKE_SHADER_DIR="${GRANIT_SMOKE_TEST_SHADER_DIR}"
    GRANIT_PBR_LIBRARY_ASSET="${granit_installed_asset_snapshot_dir}/libraries/pbr_standard.grshlib"
    GRANIT_PBR_MATERIAL_ASSET="${granit_installed_asset_snapshot_dir}/materials/pbr_standard.grmat"
)
add_dependencies(
  granit_material_test granit_shader_fixture_objects granit_test_shader_assets
)
granit_target_compile_warnings(granit_material_test)
granit_target_output_directories(granit_material_test)
add_test(NAME granit.material COMMAND granit_material_test)

set_property(TEST granit.pbr APPEND PROPERTY LABELS unit)
set_property(TEST granit.material APPEND PROPERTY LABELS gpu integration)
