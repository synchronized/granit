# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(tutorial_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(tutorial_shader_library "${tutorial_generated_dir}/pbr_standard.grshlib")
set(tutorial_shader_index "${tutorial_generated_dir}/pbr_standard.grshidx.json")
set(tutorial_material "${tutorial_generated_dir}/pbr_standard.grmat")

set(tutorial_pbr_source_dir "${PROJECT_SOURCE_DIR}/assets/sources/shaders/pbr")
granit_add_hlsl_shader_library(
  NAME tutorial_07_pbr
  MANIFEST "${tutorial_pbr_source_dir}/pbr_standard.grshlib.json"
  OUTPUT "${tutorial_shader_library}"
  INDEX "${tutorial_shader_index}"
  CACHE_DIR "${tutorial_generated_dir}/cache"
  TARGET granit_tutorial_07_shader_library
  SOURCES "${tutorial_pbr_source_dir}/pbr_standard.hlsl"
)

add_custom_command(
  OUTPUT "${tutorial_material}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${tutorial_generated_dir}"
  COMMAND "$<TARGET_FILE:granit_asset_tool>" material build
          "${PROJECT_SOURCE_DIR}/assets/sources/materials/pbr_standard.grmat.json"
          --output "${tutorial_material}"
          --shader-index "${tutorial_shader_index}"
  DEPENDS
    granit_asset_tool
    granit_tutorial_07_shader_library
    "${tutorial_shader_index}"
    "${PROJECT_SOURCE_DIR}/assets/sources/materials/pbr_standard.grmat.json"
  VERBATIM
)
add_custom_target(granit_tutorial_07_material DEPENDS "${tutorial_material}")

add_executable(granit_tutorial_07_render_pipeline main.cpp model_data.hpp)
add_dependencies(
  granit_tutorial_07_render_pipeline
  granit_tutorial_07_shader_library
  granit_tutorial_07_material
)
target_compile_definitions(
  granit_tutorial_07_render_pipeline
  PRIVATE
    GRANIT_TUTORIAL_07_SHADER_LIBRARY="${tutorial_shader_library}"
    GRANIT_TUTORIAL_07_MATERIAL="${tutorial_material}"
)
target_compile_features(granit_tutorial_07_render_pipeline PRIVATE cxx_std_20)
target_link_libraries(
  granit_tutorial_07_render_pipeline PRIVATE granit::render_pipeline granit::window
)
granit_target_compile_warnings(granit_tutorial_07_render_pipeline)
granit_target_output_directories(granit_tutorial_07_render_pipeline)

if(BUILD_TESTING)
  add_test(
    NAME granit.tutorial.07_render_pipeline
    COMMAND granit_tutorial_07_render_pipeline --smoke-test
  )
  set_tests_properties(
    granit.tutorial.07_render_pipeline
    PROPERTIES
      LABELS "tutorial;smoke;gpu;integration;platform"
      SKIP_RETURN_CODE 77
      TIMEOUT 30
  )
endif()
