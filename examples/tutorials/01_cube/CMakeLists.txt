# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(tutorial_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")

if(EMSCRIPTEN)
  set(tutorial_shader_library "${CMAKE_CURRENT_SOURCE_DIR}/cube.grshlib")
  set(tutorial_runtime_shader_library "/assets/cube.grshlib")
else()
  set(tutorial_shader_library "${tutorial_generated_dir}/cube.grshlib")
  set(tutorial_runtime_shader_library "${tutorial_shader_library}")
  granit_add_hlsl_shader_library(
    NAME tutorial_01_cube
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/cube.grshlib.json"
    OUTPUT "${tutorial_shader_library}"
    CACHE_DIR "${tutorial_generated_dir}/cache"
    TARGET granit_tutorial_01_shader_library
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/cube.hlsl"
  )
endif()

add_executable(
  granit_tutorial_01_cube
  main.cpp
  model_data.hpp
  imgui_input.cpp
  imgui_input.hpp
  imgui_resources.cpp
  imgui_resources.hpp
)
if(NOT EMSCRIPTEN)
  add_dependencies(granit_tutorial_01_cube granit_tutorial_01_shader_library)
endif()
target_compile_definitions(
  granit_tutorial_01_cube
  PRIVATE GRANIT_TUTORIAL_01_SHADER_LIBRARY="${tutorial_runtime_shader_library}"
)
target_compile_features(granit_tutorial_01_cube PRIVATE cxx_std_20)
target_link_libraries(
  granit_tutorial_01_cube
  PRIVATE granit_example_application granit::render_pipeline granit::integration_imgui
)
granit_target_compile_warnings(granit_tutorial_01_cube)
granit_target_output_directories(granit_tutorial_01_cube)

if(EMSCRIPTEN)
  include(granit_web)
  granit_target_webgpu(granit_tutorial_01_cube)
  target_link_options(
    granit_tutorial_01_cube
    PRIVATE "--preload-file=${tutorial_shader_library}@/assets/cube.grshlib"
  )
  granit_target_web_page(granit_tutorial_01_cube "${CMAKE_CURRENT_LIST_DIR}/web_shell.html")
elseif(BUILD_TESTING)
  add_test(
    NAME granit.tutorial.01_cube
    COMMAND granit_tutorial_01_cube --smoke-test
  )
  set_tests_properties(
    granit.tutorial.01_cube
    PROPERTIES
      LABELS "tutorial;smoke;gpu;integration;platform"
      SKIP_RETURN_CODE 77
      FAIL_REGULAR_EXPRESSION "ImGui Draw Data|销毁时仍有"
      TIMEOUT 30
  )
endif()
