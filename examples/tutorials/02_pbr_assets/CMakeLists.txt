# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(
  granit_tutorial_02_pbr_assets
  main.cpp
)
target_compile_features(granit_tutorial_02_pbr_assets PRIVATE cxx_std_20)
target_link_libraries(
  granit_tutorial_02_pbr_assets
  PRIVATE
    granit_example_application
    granit_example_imgui
    granit_example_model_scene
    granit::render_pipeline
)
granit_target_compile_warnings(granit_tutorial_02_pbr_assets)
granit_target_output_directories(granit_tutorial_02_pbr_assets)
granit_example_target_assets(
  TARGET granit_tutorial_02_pbr_assets
  ROOT "${PROJECT_SOURCE_DIR}/examples/assets"
  FILES
    tutorials/02_pbr_assets/Suzanne.gltf
    tutorials/02_pbr_assets/Suzanne.bin
    tutorials/02_pbr_assets/Suzanne_BaseColor.png
    tutorials/02_pbr_assets/Suzanne_MetallicRoughness.png
)

if(EMSCRIPTEN)
  include(granit_web)
  granit_target_webgpu(granit_tutorial_02_pbr_assets)
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
