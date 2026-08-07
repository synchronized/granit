# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(granit_c_version_test c_version_test.c)
target_link_libraries(granit_c_version_test PRIVATE granit::granit)
set_target_properties(granit_c_version_test PROPERTIES LINKER_LANGUAGE CXX)
granit_target_compile_warnings(granit_c_version_test)
add_test(NAME granit.c_version COMMAND granit_c_version_test)

add_library(
  granit_c_header_test
  OBJECT
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

add_library(
  granit_cpp_header_test
  OBJECT
    headers/result_hpp.cpp
    headers/types_hpp.cpp
)
target_link_libraries(granit_cpp_header_test PRIVATE granit::granit)
granit_target_compile_warnings(granit_cpp_header_test)

set_tests_properties(
  granit.c_version granit.cpp_version
  PROPERTIES ENVIRONMENT_MODIFICATION
             "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit>"
)
