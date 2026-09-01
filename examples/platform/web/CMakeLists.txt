# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_library(
  granit_model_viewer_web_shell STATIC
  asset_request.cpp
  asset_request.h
  resource_bundle.cpp
  resource_bundle.h
  resource_fetch_batch.cpp
  resource_fetch_batch.h
  web_input.cpp
  web_input.h
)
target_compile_features(granit_model_viewer_web_shell PUBLIC cxx_std_20)
target_include_directories(granit_model_viewer_web_shell PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(granit_model_viewer_web_shell PUBLIC granit_model_viewer_core)
set_target_properties(granit_model_viewer_web_shell PROPERTIES FOLDER "Examples")
granit_target_compile_warnings(granit_model_viewer_web_shell)

if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
  add_executable(
    granit_model_viewer_web_shell_test
    asset_request_test.cpp
    resource_bundle_test.cpp
    resource_fetch_batch_test.cpp
    web_input_test.cpp
  )
  target_link_libraries(
    granit_model_viewer_web_shell_test
    PRIVATE granit_model_viewer_web_shell Catch2::Catch2WithMain
  )
  granit_target_compile_warnings(granit_model_viewer_web_shell_test)
  add_test(NAME granit.example.model_viewer_web_shell COMMAND granit_model_viewer_web_shell_test)
endif()
