# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT GRANIT_HAS_NATIVE_WINDOW)
  include("${CMAKE_CURRENT_LIST_DIR}/emscripten.cmake")
  return()
endif()

# Model Viewer 的浏览器输入支撑层。浏览器资源加载已迁入 examples/common/web。

add_library(granit_sample_model_viewer_web_shell STATIC web_input.cpp web_input.h)
target_compile_features(granit_sample_model_viewer_web_shell PUBLIC cxx_std_20)
target_include_directories(
  granit_sample_model_viewer_web_shell
  PUBLIC "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer"
)
target_link_libraries(
  granit_sample_model_viewer_web_shell PUBLIC granit_sample_model_viewer_core
)
set_target_properties(
  granit_sample_model_viewer_web_shell PROPERTIES FOLDER "Examples/Samples"
)
granit_target_compile_warnings(granit_sample_model_viewer_web_shell)

if(GRANIT_TESTING_ENABLED)
  add_executable(granit_sample_model_viewer_web_shell_test web_input_test.cpp)
  target_link_libraries(
    granit_sample_model_viewer_web_shell_test
    PRIVATE granit_sample_model_viewer_web_shell Catch2::Catch2WithMain
  )
  granit_target_compile_warnings(granit_sample_model_viewer_web_shell_test)
  add_test(
    NAME granit.sample.model_viewer_web_shell
    COMMAND granit_sample_model_viewer_web_shell_test
  )
endif()
