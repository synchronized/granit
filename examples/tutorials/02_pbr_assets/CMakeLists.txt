# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(tutorial_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")

if(EMSCRIPTEN)
  set(
    tutorial_shader_library
    "${PROJECT_SOURCE_DIR}/assets/generated/installed/libraries/pbr_standard.grshlib"
  )
  set(
    tutorial_material
    "${PROJECT_SOURCE_DIR}/assets/generated/installed/materials/pbr_standard.grmat"
  )
  set(tutorial_runtime_shader_library "/assets/pbr_standard.grshlib")
  set(tutorial_runtime_material "/assets/pbr_standard.grmat")
else()
  set(tutorial_shader_library "${tutorial_generated_dir}/pbr_standard.grshlib")
  set(tutorial_material "${tutorial_generated_dir}/pbr_standard.grmat")
  set(tutorial_runtime_shader_library "${tutorial_shader_library}")
  set(tutorial_runtime_material "${tutorial_material}")

  set(tutorial_pbr_source_dir "${PROJECT_SOURCE_DIR}/assets/sources/shaders/pbr")
  granit_add_hlsl_shader_library(
    NAME tutorial_02_pbr
    MANIFEST "${tutorial_pbr_source_dir}/pbr_standard.grshlib.json"
    OUTPUT "${tutorial_shader_library}"
    CACHE_DIR "${tutorial_generated_dir}/cache"
    TARGET granit_tutorial_02_shader_library
    SOURCES "${tutorial_pbr_source_dir}/pbr_standard.hlsl"
  )

  add_custom_command(
    OUTPUT "${tutorial_material}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${tutorial_generated_dir}"
    COMMAND "$<TARGET_FILE:granit_asset_tool>" material build
            "${PROJECT_SOURCE_DIR}/assets/sources/materials/pbr_standard.grmat.json"
            --output "${tutorial_material}"
            --shader-library "${tutorial_shader_library}"
    DEPENDS
      granit_asset_tool
      granit_tutorial_02_shader_library
      "${tutorial_shader_library}"
      "${PROJECT_SOURCE_DIR}/assets/sources/materials/pbr_standard.grmat.json"
    VERBATIM
  )
  add_custom_target(granit_tutorial_02_material DEPENDS "${tutorial_material}")
endif()

add_executable(
  granit_tutorial_02_pbr_assets
  main.cpp
  model_data.hpp
  imgui_input.cpp
  imgui_input.hpp
  imgui_resources.cpp
  imgui_resources.hpp
)
if(NOT EMSCRIPTEN)
  add_dependencies(
    granit_tutorial_02_pbr_assets
    granit_tutorial_02_shader_library
    granit_tutorial_02_material
  )
endif()
target_compile_definitions(
  granit_tutorial_02_pbr_assets
  PRIVATE
    GRANIT_TUTORIAL_02_SHADER_LIBRARY="${tutorial_runtime_shader_library}"
    GRANIT_TUTORIAL_02_MATERIAL="${tutorial_runtime_material}"
)
target_compile_features(granit_tutorial_02_pbr_assets PRIVATE cxx_std_20)
target_link_libraries(
  granit_tutorial_02_pbr_assets
  PRIVATE granit_example_application granit::render_pipeline granit::integration_imgui
)
granit_target_compile_warnings(granit_tutorial_02_pbr_assets)
granit_target_output_directories(granit_tutorial_02_pbr_assets)

if(EMSCRIPTEN)
  include(granit_web)
  granit_target_webgpu(granit_tutorial_02_pbr_assets)
  target_link_options(
    granit_tutorial_02_pbr_assets
    PRIVATE
      "--preload-file=${tutorial_shader_library}@/assets/pbr_standard.grshlib"
      "--preload-file=${tutorial_material}@/assets/pbr_standard.grmat"
  )
  granit_target_web_page(granit_tutorial_02_pbr_assets "${CMAKE_CURRENT_LIST_DIR}/web_shell.html")
elseif(BUILD_TESTING)
  add_test(
    NAME granit.tutorial.02_pbr_assets
    COMMAND granit_tutorial_02_pbr_assets --smoke-test
  )
  set_tests_properties(
    granit.tutorial.02_pbr_assets
    PROPERTIES
      LABELS "tutorial;smoke;gpu;integration;platform"
      SKIP_RETURN_CODE 77
      FAIL_REGULAR_EXPRESSION "ImGui Draw Data|销毁时仍有"
      TIMEOUT 30
  )
endif()
