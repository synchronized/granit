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
    headers/renderer_h.c
    headers/result_h.c
    headers/types_h.c
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

add_executable(granit_renderer_test renderer/renderer_test.cpp)
target_link_libraries(
  granit_renderer_test
  PRIVATE granit::granit Catch2::Catch2WithMain
)
granit_target_compile_warnings(granit_renderer_test)
add_test(NAME granit.renderer COMMAND granit_renderer_test)

add_executable(
  granit_vulkan_backend_test
  backend/vulkan_backend_test.cpp
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/device.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/instance.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/loader.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/physical_device.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/result.cpp"
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
    "${GRANIT_VULKAN_HEADERS_DIR}"
)
target_compile_definitions(
  granit_vulkan_backend_test
  PRIVATE VK_NO_PROTOTYPES VOLK_NAMESPACE
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
    headers/renderer_hpp.cpp
    headers/result_hpp.cpp
    headers/types_hpp.cpp
)
target_link_libraries(granit_cpp_header_test PRIVATE granit::granit)
granit_target_compile_warnings(granit_cpp_header_test)

set_tests_properties(
  granit.c_version granit.cpp_version granit.handle_table granit.renderer granit.vulkan_backend
  PROPERTIES ENVIRONMENT_MODIFICATION
             "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit>"
)
