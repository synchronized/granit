# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(sample_model "${CMAKE_CURRENT_SOURCE_DIR}/assets/tutorial_cube.gltf")

add_executable(granit_sample_model_loading main.cpp)
target_compile_definitions(
  granit_sample_model_loading PRIVATE GRANIT_SAMPLE_MODEL="${sample_model}"
)
target_compile_features(granit_sample_model_loading PRIVATE cxx_std_20)
target_link_libraries(
  granit_sample_model_loading PRIVATE granit_example_model_scene granit::window
)
granit_target_compile_warnings(granit_sample_model_loading)
granit_target_output_directories(granit_sample_model_loading)

if(BUILD_TESTING)
  add_test(
    NAME granit.sample.model_loading
    COMMAND granit_sample_model_loading --smoke-test
  )
  set_tests_properties(
    granit.sample.model_loading
    PROPERTIES
      LABELS "sample;smoke;gpu;integration;platform"
      SKIP_RETURN_CODE 77
      TIMEOUT 30
  )
endif()
