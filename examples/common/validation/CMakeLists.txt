# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 示例共享的截图/视觉回归比较，无 Granit 与第三方依赖。

add_library(granit_example_validation STATIC screenshot_comparison.cpp screenshot_comparison.h)
target_compile_features(granit_example_validation PUBLIC cxx_std_20)
target_include_directories(
  granit_example_validation PUBLIC "${PROJECT_SOURCE_DIR}/examples/common"
)
set_target_properties(granit_example_validation PROPERTIES FOLDER "Examples")
granit_target_compile_warnings(granit_example_validation)

if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
  add_executable(granit_example_validation_test screenshot_comparison_test.cpp)
  target_link_libraries(
    granit_example_validation_test PRIVATE granit_example_validation Catch2::Catch2WithMain
  )
  granit_target_compile_warnings(granit_example_validation_test)
  add_test(NAME granit.example.validation COMMAND granit_example_validation_test)
endif()
