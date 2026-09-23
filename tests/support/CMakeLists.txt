# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(granit_smoke_asset_dir "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke")
set(
  granit_render_pipeline_test_package
  "${GRANIT_TEST_BINARY_DIR}/assets/render_pipeline_untextured.grmat"
)
add_custom_command(
  OUTPUT "${granit_render_pipeline_test_package}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${GRANIT_TEST_BINARY_DIR}/assets"
  COMMAND
    granit_asset_tool material build
    "${granit_smoke_asset_dir}/render_pipeline_untextured.grmat.json"
    --output "${granit_render_pipeline_test_package}" --shader-library
    "${GRANIT_PBR_TEST_SHADER_LIBRARY}"
  DEPENDS
    granit_asset_tool
    granit_test_shader_assets
    "${granit_smoke_asset_dir}/render_pipeline_untextured.grmat.json"
    "${GRANIT_PBR_TEST_SHADER_LIBRARY}"
  VERBATIM
)
add_custom_target(
  granit_render_pipeline_test_package DEPENDS "${granit_render_pipeline_test_package}"
)

set(granit_render_pipeline_test_package "${granit_render_pipeline_test_package}" PARENT_SCOPE)
