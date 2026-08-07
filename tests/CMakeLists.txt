# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(granit_c_version_test c_version_test.c)
target_link_libraries(granit_c_version_test PRIVATE granit::granit)
set_target_properties(granit_c_version_test PROPERTIES LINKER_LANGUAGE CXX)
granit_target_compile_warnings(granit_c_version_test)
add_test(NAME granit.c_version COMMAND granit_c_version_test)

add_executable(granit_cpp_version_test cpp_version_test.cpp)
target_link_libraries(
  granit_cpp_version_test
  PRIVATE granit::granit Catch2::Catch2WithMain
)
granit_target_compile_warnings(granit_cpp_version_test)
if(MSVC)
  target_compile_options(granit_cpp_version_test PRIVATE /utf-8)
endif()
add_test(NAME granit.cpp_version COMMAND granit_cpp_version_test)

set_tests_properties(
  granit.c_version granit.cpp_version
  PROPERTIES ENVIRONMENT_MODIFICATION
             "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit>"
)
