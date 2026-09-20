# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set_property(DIRECTORY APPEND PROPERTY LABELS unit)

add_executable(granit_core_api_c_test core_api_c_test.c)
target_compile_features(granit_core_api_c_test PRIVATE c_std_11)
target_link_libraries(granit_core_api_c_test PRIVATE granit::granit Unity::Unity)
set_target_properties(granit_core_api_c_test PROPERTIES LINKER_LANGUAGE CXX)
granit_target_compile_warnings(granit_core_api_c_test)
granit_target_output_directories(granit_core_api_c_test)
add_test(NAME granit.core_api.c COMMAND granit_core_api_c_test)

add_executable(granit_cpp_version_test version_cpp_test.cpp)
target_link_libraries(
  granit_cpp_version_test
  PRIVATE granit::granit Catch2::Catch2WithMain
)
granit_target_compile_warnings(granit_cpp_version_test)
add_test(NAME granit.cpp_version COMMAND granit_cpp_version_test)

granit_target_output_directories(granit_cpp_version_test)

set_tests_properties(granit.core_api.c granit.cpp_version PROPERTIES
  ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::granit>"
)

add_executable(
  granit_sha256_test
  sha256_test.cpp
  "${PROJECT_SOURCE_DIR}/src/core/sha256.cpp"
)
target_include_directories(
  granit_sha256_test PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries(granit_sha256_test PRIVATE Catch2::Catch2WithMain)
granit_target_compile_warnings(granit_sha256_test)
add_test(NAME granit.sha256 COMMAND granit_sha256_test)
set_property(TEST granit.sha256 APPEND PROPERTY LABELS unit)

add_executable(
  granit_utf8_test
  utf8_test.cpp
  "${PROJECT_SOURCE_DIR}/src/core/utf8.cpp"
)
target_include_directories(
  granit_utf8_test PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries(granit_utf8_test PRIVATE Catch2::Catch2WithMain)
granit_target_compile_warnings(granit_utf8_test)
add_test(NAME granit.utf8 COMMAND granit_utf8_test)
set_property(TEST granit.utf8 APPEND PROPERTY LABELS unit)

add_executable(granit_c_version_test version_c_test.c)
target_link_libraries(granit_c_version_test PRIVATE granit::granit Unity::Unity)
set_target_properties(granit_c_version_test PROPERTIES LINKER_LANGUAGE CXX)
granit_target_compile_warnings(granit_c_version_test)
add_test(NAME granit.c_version COMMAND granit_c_version_test)


add_executable(
  granit_handle_table_test
  handle_table_test.cpp
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
  granit_async_operation_state_test
  async_operation_state_test.cpp
  "${PROJECT_SOURCE_DIR}/src/core/async_operation_state.cpp"
)
target_include_directories(granit_async_operation_state_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
  granit_async_operation_state_test PRIVATE granit::granit Catch2::Catch2WithMain
)
granit_target_compile_warnings(granit_async_operation_state_test)
add_test(NAME granit.async_operation_state COMMAND granit_async_operation_state_test)

add_executable(
  granit_resource_validation_test
  resource_validation_test.cpp
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
  diagnostic_sink_test.cpp
  lifecycle_validation_test.cpp
  "${PROJECT_SOURCE_DIR}/src/core/diagnostic_sink.cpp"
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
  retirement_queue_test.cpp
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

add_executable(granit_device_status_test device_status_test.cpp)
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

set(
  granit_core_test_targets
  granit_c_version_test
  granit_handle_table_test
  granit_async_operation_state_test
  granit_resource_validation_test
  granit_lifecycle_validation_test
  granit_retirement_queue_test
  granit_device_status_test
)
foreach(target IN LISTS granit_core_test_targets)
  granit_target_output_directories(${target})
endforeach()

set_tests_properties(
  granit.c_version
  granit.handle_table
  granit.async_operation_state
  granit.resource_validation
  granit.lifecycle_validation
  granit.retirement_queue
  granit.device_status
  PROPERTIES ENVIRONMENT_MODIFICATION
             "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit>"
)
