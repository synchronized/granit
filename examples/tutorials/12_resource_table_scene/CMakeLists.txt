# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(granit_tutorial_12_resource_table_scene main.cpp)
target_compile_features(granit_tutorial_12_resource_table_scene PRIVATE cxx_std_20)
target_link_libraries(
  granit_tutorial_12_resource_table_scene
  PRIVATE
    granit_example_application
    granit_example_camera
    granit_example_tutorial
    granit_example_gltf_rendering
    granit::render_pipeline
)
granit_target_compile_warnings(granit_tutorial_12_resource_table_scene)
granit_target_output_directories(granit_tutorial_12_resource_table_scene)
granit_example_target_assets(
  TARGET granit_tutorial_12_resource_table_scene
  ROOT "${PROJECT_SOURCE_DIR}/examples/assets"
  FILES
    tutorials/12_resource_table_scene/ResourceTableGallery.gltf
    tutorials/12_resource_table_scene/ResourceTableGallery.bin
    tutorials/12_resource_table_scene/textures/Gallery_BaseColor_00.png
    tutorials/12_resource_table_scene/textures/Gallery_BaseColor_01.png
    tutorials/12_resource_table_scene/textures/Gallery_BaseColor_02.png
    tutorials/12_resource_table_scene/textures/Gallery_BaseColor_03.png
    tutorials/12_resource_table_scene/textures/Gallery_BaseColor_04.png
    tutorials/12_resource_table_scene/textures/Gallery_BaseColor_05.png
    tutorials/12_resource_table_scene/textures/Gallery_BaseColor_06.png
    tutorials/12_resource_table_scene/textures/Gallery_BaseColor_07.png
    tutorials/12_resource_table_scene/textures/Gallery_BaseColor_08.png
    tutorials/12_resource_table_scene/textures/Gallery_BaseColor_09.png
    tutorials/12_resource_table_scene/textures/Gallery_BaseColor_10.png
    tutorials/12_resource_table_scene/textures/Gallery_BaseColor_11.png
    tutorials/12_resource_table_scene/textures/Gallery_BaseColor_12.png
    tutorials/12_resource_table_scene/textures/Gallery_BaseColor_13.png
    tutorials/12_resource_table_scene/textures/Gallery_BaseColor_14.png
    tutorials/12_resource_table_scene/textures/Gallery_BaseColor_15.png
    tutorials/12_resource_table_scene/textures/Gallery_MetallicRoughness_00.png
    tutorials/12_resource_table_scene/textures/Gallery_MetallicRoughness_01.png
    tutorials/12_resource_table_scene/textures/Gallery_MetallicRoughness_02.png
    tutorials/12_resource_table_scene/textures/Gallery_MetallicRoughness_03.png
    tutorials/12_resource_table_scene/textures/Gallery_MetallicRoughness_04.png
    tutorials/12_resource_table_scene/textures/Gallery_MetallicRoughness_05.png
    tutorials/12_resource_table_scene/textures/Gallery_MetallicRoughness_06.png
    tutorials/12_resource_table_scene/textures/Gallery_MetallicRoughness_07.png
    tutorials/12_resource_table_scene/textures/Gallery_MetallicRoughness_08.png
    tutorials/12_resource_table_scene/textures/Gallery_MetallicRoughness_09.png
    tutorials/12_resource_table_scene/textures/Gallery_MetallicRoughness_10.png
    tutorials/12_resource_table_scene/textures/Gallery_MetallicRoughness_11.png
    tutorials/12_resource_table_scene/textures/Gallery_MetallicRoughness_12.png
    tutorials/12_resource_table_scene/textures/Gallery_MetallicRoughness_13.png
    tutorials/12_resource_table_scene/textures/Gallery_MetallicRoughness_14.png
    tutorials/12_resource_table_scene/textures/Gallery_MetallicRoughness_15.png
)
if(EMSCRIPTEN)
  include(granit_web)
  granit_target_webgpu(granit_tutorial_12_resource_table_scene)
  granit_target_web_page(granit_tutorial_12_resource_table_scene "${CMAKE_CURRENT_LIST_DIR}/web_shell.html")
elseif(BUILD_TESTING)
  add_test(NAME granit.tutorial.12_resource_table_scene COMMAND granit_tutorial_12_resource_table_scene --smoke-test)
  set_tests_properties(granit.tutorial.12_resource_table_scene PROPERTIES LABELS "tutorial;smoke;gpu;integration;platform"
    SKIP_RETURN_CODE 77 FAIL_REGULAR_EXPRESSION "ImGui Draw Data|销毁时仍有" TIMEOUT 30)
endif()
