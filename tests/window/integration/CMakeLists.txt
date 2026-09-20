# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(granit_window_hdr_test hdr_window_test.cpp)
add_dependencies(granit_window_hdr_test granit_test_shader_assets)
target_sources(
  granit_window_hdr_test PRIVATE "${PROJECT_SOURCE_DIR}/tests/support/pbr_test_support.cpp"
)
target_include_directories(granit_window_hdr_test PRIVATE "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(granit_window_hdr_test PRIVATE granit::lighting granit::window)
target_compile_definitions(
  granit_window_hdr_test
  PRIVATE
    GRANIT_PBR_SHADER_DIR="${GRANIT_PBR_TEST_SHADER_DIR}"
    GRANIT_PIPELINE_SHADER_DIR="${GRANIT_PIPELINE_TEST_SHADER_DIR}"
)
granit_target_compile_warnings(granit_window_hdr_test)
granit_target_output_directories(granit_window_hdr_test)
add_test(NAME granit.window.hdr COMMAND granit_window_hdr_test --smoke-test)
set_tests_properties(granit.window.hdr PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
set_property(TEST granit.window.hdr APPEND PROPERTY LABELS gpu integration platform)
