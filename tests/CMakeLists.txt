# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(granit_c_version_test c_version_test.c)
target_link_libraries(granit_c_version_test PRIVATE granit::granit Unity::Unity)
set_target_properties(granit_c_version_test PROPERTIES LINKER_LANGUAGE CXX)
granit_target_compile_warnings(granit_c_version_test)
add_test(NAME granit.c_version COMMAND granit_c_version_test)

add_library(
  granit_c_header_test
  OBJECT
    headers/command_recorder_h.c
    headers/pipeline_h.c
    headers/buffer_h.c
    headers/renderer_h.c
    headers/render_target_h.c
    headers/resource_types_h.c
    headers/sampler_h.c
    headers/shader_h.c
    headers/result_h.c
    headers/surface_h.c
    headers/swapchain_h.c
    headers/texture_h.c
    headers/types_h.c
    headers/upload_batch_h.c
)
target_link_libraries(granit_c_header_test PRIVATE granit::granit)
granit_target_compile_warnings(granit_c_header_test)

add_executable(granit_cpp_version_test cpp_version_test.cpp)
target_link_libraries(
  granit_cpp_version_test
  PRIVATE granit::granit Catch2::Catch2WithMain
)
granit_target_compile_warnings(granit_cpp_version_test)
add_test(NAME granit.cpp_version COMMAND granit_cpp_version_test)

add_executable(
  granit_handle_table_test
  core/handle_table_test.cpp
  "${PROJECT_SOURCE_DIR}/src/core/handle_table.cpp"
)
target_include_directories(granit_handle_table_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
  granit_handle_table_test
  PRIVATE granit::granit Catch2::Catch2WithMain
)
granit_target_compile_warnings(granit_handle_table_test)
add_test(NAME granit.handle_table COMMAND granit_handle_table_test)

add_executable(
  granit_resource_validation_test
  core/resource_validation_test.cpp
  "${PROJECT_SOURCE_DIR}/src/core/resource_validation.cpp"
)
target_include_directories(granit_resource_validation_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
  granit_resource_validation_test
  PRIVATE granit::granit Catch2::Catch2WithMain
)
granit_target_compile_warnings(granit_resource_validation_test)
add_test(NAME granit.resource_validation COMMAND granit_resource_validation_test)

add_executable(
  granit_lifecycle_validation_test
  core/lifecycle_validation_test.cpp
  "${PROJECT_SOURCE_DIR}/src/core/lifecycle_validation.cpp"
)
target_include_directories(
  granit_lifecycle_validation_test
  PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(granit_lifecycle_validation_test PRIVATE Catch2::Catch2WithMain)
target_compile_options(
  granit_lifecycle_validation_test
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
granit_target_compile_warnings(granit_lifecycle_validation_test)
add_test(NAME granit.lifecycle_validation COMMAND granit_lifecycle_validation_test)

add_executable(
  granit_retirement_queue_test
  core/retirement_queue_test.cpp
  "${PROJECT_SOURCE_DIR}/src/core/retirement_queue.cpp"
)
target_include_directories(granit_retirement_queue_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(granit_retirement_queue_test PRIVATE Catch2::Catch2WithMain)
target_compile_options(
  granit_retirement_queue_test
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
granit_target_compile_warnings(granit_retirement_queue_test)
add_test(NAME granit.retirement_queue COMMAND granit_retirement_queue_test)

add_executable(granit_device_status_test core/device_status_test.cpp)
target_include_directories(
  granit_device_status_test
  PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(granit_device_status_test PRIVATE Catch2::Catch2WithMain)
target_compile_options(
  granit_device_status_test
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
granit_target_compile_warnings(granit_device_status_test)
add_test(NAME granit.device_status COMMAND granit_device_status_test)

add_executable(
  granit_render_graph_test
  render_graph/graph_compiler_test.cpp
  render_graph/serial_graph_test.cpp
  "${PROJECT_SOURCE_DIR}/src/render_graph/graph_compiler.cpp"
  "${PROJECT_SOURCE_DIR}/src/render_graph/serial_graph.cpp"
)
target_include_directories(granit_render_graph_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(granit_render_graph_test PRIVATE granit::granit Catch2::Catch2WithMain)
target_compile_options(
  granit_render_graph_test
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
granit_target_compile_warnings(granit_render_graph_test)
add_test(NAME granit.render_graph COMMAND granit_render_graph_test)

add_executable(
  granit_renderer_test
  renderer/command_recorder_test.cpp
  renderer/pipeline_test.cpp
  renderer/buffer_test.cpp
  renderer/renderer_test.cpp
  renderer/sampler_test.cpp
  renderer/shader_test.cpp
  renderer/surface_test.cpp
  renderer/swapchain_test.cpp
  renderer/texture_test.cpp
  renderer/upload_batch_test.cpp
)
target_link_libraries(
  granit_renderer_test
  PRIVATE
    granit::granit
    Catch2::Catch2WithMain
    $<$<PLATFORM_ID:Windows>:user32>
)
target_compile_definitions(
  granit_renderer_test
  PRIVATE GRANIT_TEST_ASSET_DIR="${PROJECT_SOURCE_DIR}/tests/assets"
)
granit_target_compile_warnings(granit_renderer_test)
add_test(NAME granit.renderer COMMAND granit_renderer_test)

add_executable(
  granit_vulkan_backend_test
  backend/vulkan_backend_test.cpp
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/device.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/frame_context.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/instance.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/loader.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/memory_allocator.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/physical_device.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/result.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/surface.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/swapchain.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/vma_implementation.cpp"
  $<TARGET_OBJECTS:granit_volk>
)
target_include_directories(
  granit_vulkan_backend_test
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
)
target_include_directories(
  granit_vulkan_backend_test
  SYSTEM PRIVATE
    "${GRANIT_VOLK_DIR}"
    "${GRANIT_VMA_DIR}"
    "${GRANIT_VULKAN_HEADERS_DIR}"
)
target_compile_definitions(
  granit_vulkan_backend_test
  PRIVATE
    VK_NO_PROTOTYPES
    VOLK_NAMESPACE
    VMA_DYNAMIC_VULKAN_FUNCTIONS=0
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_VULKAN_VERSION=1003000
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
)
target_link_libraries(
  granit_vulkan_backend_test
  PRIVATE granit::granit Catch2::Catch2WithMain ${CMAKE_DL_LIBS}
)
granit_target_compile_warnings(granit_vulkan_backend_test)
add_test(NAME granit.vulkan_backend COMMAND granit_vulkan_backend_test)

add_library(
  granit_cpp_header_test
  OBJECT
    headers/command_recorder_hpp.cpp
    headers/pipeline_hpp.cpp
    headers/buffer_hpp.cpp
    headers/renderer_hpp.cpp
    headers/render_target_hpp.cpp
    headers/resource_types_hpp.cpp
    headers/sampler_hpp.cpp
    headers/shader_hpp.cpp
    headers/result_hpp.cpp
    headers/surface_hpp.cpp
    headers/swapchain_hpp.cpp
    headers/texture_hpp.cpp
    headers/types_hpp.cpp
    headers/upload_batch_hpp.cpp
)
target_link_libraries(granit_cpp_header_test PRIVATE granit::granit)
granit_target_compile_warnings(granit_cpp_header_test)

set(
  granit_test_executable_targets
  granit_c_version_test
  granit_cpp_version_test
  granit_handle_table_test
  granit_resource_validation_test
  granit_lifecycle_validation_test
  granit_retirement_queue_test
  granit_device_status_test
  granit_render_graph_test
  granit_renderer_test
  granit_vulkan_backend_test
)
foreach(target IN LISTS granit_test_executable_targets)
  granit_target_output_directories(${target})
endforeach()

set_tests_properties(
  granit.c_version granit.cpp_version granit.handle_table granit.resource_validation
  granit.lifecycle_validation granit.retirement_queue granit.device_status granit.renderer
  granit.vulkan_backend
  PROPERTIES ENVIRONMENT_MODIFICATION
             "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit>"
)
