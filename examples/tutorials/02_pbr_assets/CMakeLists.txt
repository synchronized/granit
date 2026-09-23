# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(tutorial_model_dir "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(tutorial_model "${tutorial_model_dir}/Suzanne.gltf")
if(EMSCRIPTEN)
  set(tutorial_runtime_model "/assets/Suzanne.gltf")
else()
  set(tutorial_runtime_model "${tutorial_model}")
endif()

add_executable(
  granit_tutorial_02_pbr_assets
  main.cpp
  imgui_input.cpp
  imgui_input.hpp
  imgui_resources.cpp
  imgui_resources.hpp
)
target_compile_definitions(
  granit_tutorial_02_pbr_assets
  PRIVATE GRANIT_TUTORIAL_02_MODEL="${tutorial_runtime_model}"
)
target_compile_features(granit_tutorial_02_pbr_assets PRIVATE cxx_std_20)
target_link_libraries(
  granit_tutorial_02_pbr_assets
  PRIVATE
    granit_example_application
    granit_example_model_scene
    granit::render_pipeline
    granit::integration_imgui
)
granit_target_compile_warnings(granit_tutorial_02_pbr_assets)
granit_target_output_directories(granit_tutorial_02_pbr_assets)

if(EMSCRIPTEN)
  include(granit_web)
  granit_target_webgpu(granit_tutorial_02_pbr_assets)
  target_link_options(
    granit_tutorial_02_pbr_assets
    PRIVATE
      "--preload-file=${tutorial_model}@/assets/Suzanne.gltf"
      "--preload-file=${tutorial_model_dir}/Suzanne.bin@/assets/Suzanne.bin"
      "--preload-file=${tutorial_model_dir}/Suzanne_BaseColor.png@/assets/Suzanne_BaseColor.png"
      "--preload-file=${tutorial_model_dir}/Suzanne_MetallicRoughness.png@/assets/Suzanne_MetallicRoughness.png"
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