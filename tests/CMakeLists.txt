# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(GRANIT_TEST_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

add_subdirectory(core)
add_subdirectory(headers)
add_subdirectory(window)

if(NOT GRANIT_HAS_NATIVE_WINDOW)
  add_subdirectory(web)
  return()
endif()

add_subdirectory(asset_tools)
add_subdirectory(backend)
add_subdirectory(render_graph)
add_subdirectory(math)
add_subdirectory(scene)
add_subdirectory(integrations)
add_subdirectory(lighting)
add_subdirectory(pipeline)
add_subdirectory(material)
add_subdirectory(renderer)
add_subdirectory(abi)

set(granit_smoke_asset_dir "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke")
set(
  granit_render_pipeline_test_package
  "${CMAKE_CURRENT_BINARY_DIR}/assets/render_pipeline_untextured.grmat"
)
add_custom_command(
  OUTPUT "${granit_render_pipeline_test_package}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets"
  COMMAND
    granit_asset_tool material build
    "${granit_smoke_asset_dir}/render_pipeline_untextured.grmat.json"
    --output "${granit_render_pipeline_test_package}" --shader-index
    "${GRANIT_PBR_TEST_SHADER_INDEX}"
  DEPENDS
    granit_asset_tool
    granit_test_shader_assets
    "${granit_smoke_asset_dir}/render_pipeline_untextured.grmat.json"
    "${GRANIT_PBR_TEST_SHADER_INDEX}"
  VERBATIM
)
add_custom_target(
  granit_render_pipeline_test_package DEPENDS "${granit_render_pipeline_test_package}"
)

add_subdirectory(smoke)
add_subdirectory(gpu)

add_test(
  NAME granit.documentation
  COMMAND
    "${CMAKE_COMMAND}"
    "-DGRANIT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    -P "${PROJECT_SOURCE_DIR}/tests/cmake/check_documentation.cmake"
)

add_test(
  NAME granit.build_tree_consumers
  COMMAND
    "${CMAKE_COMMAND}" "-DGRANIT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DGRANIT_TEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/build-tree-consumers"
    -P "${PROJECT_SOURCE_DIR}/tests/cmake/check_build_tree_consumers.cmake"
)

add_test(
  NAME granit.release_version
  COMMAND
    "${CMAKE_COMMAND}"
    "-DGRANIT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DGRANIT_RELEASE_TAG=v${PROJECT_VERSION}"
    -P "${PROJECT_SOURCE_DIR}/tests/cmake/check_release_version.cmake"
)
add_test(
  NAME granit.release_version_mismatch
  COMMAND
    "${CMAKE_COMMAND}"
    "-DGRANIT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    -DGRANIT_RELEASE_TAG=v999.0.0
    -P "${PROJECT_SOURCE_DIR}/tests/cmake/check_release_version.cmake"
)
set_tests_properties(granit.release_version_mismatch PROPERTIES WILL_FAIL TRUE)

set_property(TEST granit.documentation APPEND PROPERTY LABELS unit)
set_property(
  TEST granit.build_tree_consumers granit.release_version granit.release_version_mismatch
  APPEND PROPERTY LABELS package release
)
