# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(tutorial_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(tutorial_shader_library "${tutorial_generated_dir}/texture.grshlib")
set(tutorial_shader_index "${tutorial_generated_dir}/texture.grshidx.json")
set(tutorial_shader_ids "${tutorial_generated_dir}/texture_shader_ids.inc")

granit_add_hlsl_shader_library(
  NAME tutorial_03_texture
  MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/texture.grshlib.json"
  OUTPUT "${tutorial_shader_library}"
  INDEX "${tutorial_shader_index}"
  CACHE_DIR "${tutorial_generated_dir}/cache"
  TARGET granit_tutorial_03_shader_library
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/texture.hlsl"
)

add_custom_command(
  OUTPUT "${tutorial_shader_ids}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${tutorial_generated_dir}"
  COMMAND "$<TARGET_FILE:granit_asset_tool>" shader index-ids
          --index "${tutorial_shader_index}"
          --shader texture_vertex_id=texture.vertex
          --shader texture_fragment_id=texture.fragment
          --output "${tutorial_shader_ids}"
  DEPENDS granit_asset_tool granit_tutorial_03_shader_library "${tutorial_shader_index}"
  VERBATIM
)

add_executable(granit_tutorial_03_texture main.cpp "${tutorial_shader_ids}")
add_dependencies(granit_tutorial_03_texture granit_tutorial_03_shader_library)
target_include_directories(granit_tutorial_03_texture PRIVATE "${tutorial_generated_dir}")
target_compile_definitions(
  granit_tutorial_03_texture
  PRIVATE GRANIT_TUTORIAL_03_SHADER_LIBRARY="${tutorial_shader_library}"
)
target_compile_features(granit_tutorial_03_texture PRIVATE cxx_std_20)
target_link_libraries(granit_tutorial_03_texture PRIVATE granit::granit granit::window)
granit_target_compile_warnings(granit_tutorial_03_texture)
granit_target_output_directories(granit_tutorial_03_texture)

if(BUILD_TESTING)
  add_test(
    NAME granit.tutorial.03_texture
    COMMAND granit_tutorial_03_texture --smoke-test
  )
  set_tests_properties(
    granit.tutorial.03_texture
    PROPERTIES
      LABELS "tutorial;smoke;gpu;integration;platform"
      SKIP_RETURN_CODE 77
      TIMEOUT 30
  )
endif()
