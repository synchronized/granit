# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(granit_tutorial_01_window main.cpp)

target_compile_features(granit_tutorial_01_window PRIVATE cxx_std_20)
target_link_libraries(granit_tutorial_01_window PRIVATE granit::granit granit::window)
granit_target_compile_warnings(granit_tutorial_01_window)
granit_target_output_directories(granit_tutorial_01_window)

if(BUILD_TESTING)
  add_test(
    NAME granit.tutorial.01_window
    COMMAND granit_tutorial_01_window --smoke-test
  )
  set_tests_properties(
    granit.tutorial.01_window
    PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77
               FAIL_REGULAR_EXPRESSION "\\[granit\\]\\[vulkan\\]"
               LABELS "tutorial;smoke;gpu;integration;platform"
  )
endif()
