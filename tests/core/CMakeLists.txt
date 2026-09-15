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
