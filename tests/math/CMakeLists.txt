# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set_property(DIRECTORY APPEND PROPERTY LABELS unit)

add_executable(granit_math_test math_test.cpp)
target_include_directories(granit_math_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(granit_math_test PRIVATE granit::math Catch2::Catch2WithMain)
target_compile_options(granit_math_test PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>)
granit_target_compile_warnings(granit_math_test)
add_test(NAME granit.math COMMAND granit_math_test)

granit_target_output_directories(granit_math_test)
