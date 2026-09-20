# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(
  granit_renderer_test
  command_recorder_test.cpp
  pipeline_test.cpp
  buffer_test.cpp
  renderer_test.cpp
  sampler_test.cpp
  shader_test.cpp
  shader_library_test.cpp
  surface_test.cpp
  swapchain_test.cpp
  texture_test.cpp
  texture_asset_test.cpp
  upload_batch_test.cpp
  readback_batch_test.cpp
  pipeline_warmup_test.cpp
)
if(GRANIT_HAS_XCB)
  target_sources(granit_renderer_test PRIVATE xcb_swapchain_test.cpp)
  target_include_directories(granit_renderer_test SYSTEM PRIVATE "${GRANIT_XCB_INCLUDE_DIR}")
  target_link_libraries(granit_renderer_test PRIVATE "${GRANIT_XCB_LIBRARY}")
endif()
if(GRANIT_HAS_WAYLAND_SHELL)
  target_sources(
    granit_renderer_test
    PRIVATE wayland_swapchain_test.cpp $<TARGET_OBJECTS:granit_xdg_shell_protocol>
  )
  add_dependencies(granit_renderer_test granit_xdg_shell_protocol)
  target_include_directories(
    granit_renderer_test
    SYSTEM PRIVATE "${GRANIT_WAYLAND_INCLUDE_DIR}" "${granit_wayland_protocol_dir}"
  )
  target_link_libraries(granit_renderer_test PRIVATE "${GRANIT_WAYLAND_LIBRARY}")
endif()
target_link_libraries(
  granit_renderer_test
  PRIVATE
    granit::granit
    Catch2::Catch2WithMain
    $<TARGET_OBJECTS:granit_asset_tools_shader_object_storage>
    $<TARGET_OBJECTS:granit_asset_format_shader>
    $<$<PLATFORM_ID:Windows>:user32>
)
target_include_directories(
  granit_renderer_test
  PRIVATE
    "${PROJECT_SOURCE_DIR}/tests"
    "${PROJECT_SOURCE_DIR}/tools"
    "${PROJECT_SOURCE_DIR}/src"
)
target_compile_definitions(
  granit_renderer_test
  PRIVATE
    GRANIT_SHADER_OBJECT_FIXTURE_DIR="${GRANIT_TEST_BINARY_DIR}/asset_tools/test-objects"
    GRANIT_TEST_ASSET_DIR="${granit_test_shader_fixture_dir}"
    GRANIT_TEST_ARTIFACT_DIR="${CMAKE_BINARY_DIR}/test-artifacts"
)
add_dependencies(granit_renderer_test granit_shader_fixture_objects)
granit_target_compile_warnings(granit_renderer_test)
granit_target_output_directories(granit_renderer_test)
add_test(NAME granit.renderer COMMAND granit_renderer_test)
add_test(
  NAME granit.renderer.lifecycle_diagnostic
  COMMAND granit_renderer_test "[renderer][lifecycle]"
)
set_tests_properties(
  granit.renderer.lifecycle_diagnostic
  PROPERTIES PASS_REGULAR_EXPRESSION "\\[granit\\]\\[lifecycle\\].*Buffer=1"
)
set_property(TEST granit.renderer granit.renderer.lifecycle_diagnostic APPEND PROPERTY LABELS gpu integration)
set_tests_properties(
  granit.renderer
  PROPERTIES ENVIRONMENT_MODIFICATION
             "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit>"
)
