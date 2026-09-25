# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set_property(DIRECTORY APPEND PROPERTY LABELS unit)

add_executable(granit_math_test math_test.cpp)
target_link_libraries(granit_math_test PRIVATE granit::granit Catch2::Catch2WithMain)
target_compile_options(granit_math_test PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>)
granit_target_compile_warnings(granit_math_test)
add_test(NAME granit.math.types COMMAND granit_math_test)

granit_target_output_directories(granit_math_test)
