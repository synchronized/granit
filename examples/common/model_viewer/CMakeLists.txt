# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_library(granit_example_model_viewer_support STATIC gpu_scene.cpp)
target_compile_features(granit_example_model_viewer_support PUBLIC cxx_std_20)
target_include_directories(
  granit_example_model_viewer_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
target_link_libraries(
  granit_example_model_viewer_support PUBLIC granit_example_gltf_support granit::render_pipeline
)
set_target_properties(granit_example_model_viewer_support PROPERTIES FOLDER "Examples")
granit_target_compile_warnings(granit_example_model_viewer_support)

if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
  add_executable(granit_example_model_viewer_support_test gpu_scene_test.cpp)
  target_link_libraries(
    granit_example_model_viewer_support_test
    PRIVATE granit_example_model_viewer_support Catch2::Catch2WithMain
  )
  granit_target_compile_warnings(granit_example_model_viewer_support_test)
  add_test(
    NAME granit.example.model_viewer_support COMMAND granit_example_model_viewer_support_test
  )
endif()
