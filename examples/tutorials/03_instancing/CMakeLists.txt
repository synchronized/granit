# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(tutorial_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(tutorial_shader_snapshot "${CMAKE_CURRENT_SOURCE_DIR}/instancing.grshlib")
set(tutorial_shader_include "${tutorial_generated_dir}/instancing.grshlib.inc")

if(EMSCRIPTEN)
  set(tutorial_shader_library "${tutorial_shader_snapshot}")
else()
  set(tutorial_shader_library "${tutorial_generated_dir}/instancing.grshlib")
  granit_add_hlsl_shader_library(
    NAME tutorial_03_instancing
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/instancing.grshlib.json"
    OUTPUT "${tutorial_shader_library}"
    CACHE_DIR "${tutorial_generated_dir}/cache"
    REFERENCE "${tutorial_shader_snapshot}"
    TARGET granit_tutorial_03_shader_library
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/instancing.hlsl"
  )
endif()

add_custom_command(
  OUTPUT "${tutorial_shader_include}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${tutorial_generated_dir}"
  COMMAND
    "${CMAKE_COMMAND}" "-DINPUT=${tutorial_shader_library}"
    "-DOUTPUT=${tutorial_shader_include}"
    -P "${PROJECT_SOURCE_DIR}/cmake/assets/embed_binary.cmake"
  DEPENDS "${tutorial_shader_library}" "${PROJECT_SOURCE_DIR}/cmake/assets/embed_binary.cmake"
  COMMENT "内嵌 03 Instancing Shader Library"
  VERBATIM
)

add_executable(
  granit_tutorial_03_instancing
  main.cpp
  model_data.hpp
  shader_archive.cpp
  shader_archive.h
  "${tutorial_shader_include}"
)
if(NOT EMSCRIPTEN)
  add_dependencies(granit_tutorial_03_instancing granit_tutorial_03_shader_library)
endif()
target_include_directories(
  granit_tutorial_03_instancing PRIVATE "${tutorial_generated_dir}"
)
target_compile_features(granit_tutorial_03_instancing PRIVATE cxx_std_20)
target_link_libraries(
  granit_tutorial_03_instancing
  PRIVATE
    granit_example_application
    granit::render_pipeline
)
granit_target_compile_warnings(granit_tutorial_03_instancing)
granit_target_output_directories(granit_tutorial_03_instancing)

if(EMSCRIPTEN)
  include(granit_web)
  granit_target_webgpu(granit_tutorial_03_instancing)
  granit_target_web_page(granit_tutorial_03_instancing "${CMAKE_CURRENT_LIST_DIR}/web_shell.html")
elseif(BUILD_TESTING)
  add_test(
    NAME granit.tutorial.03_instancing
    COMMAND granit_tutorial_03_instancing --smoke-test
  )
  set_tests_properties(
    granit.tutorial.03_instancing
    PROPERTIES
      LABELS "tutorial;smoke;gpu;integration;platform"
      SKIP_RETURN_CODE 77
      FAIL_REGULAR_EXPRESSION "销毁时仍有"
      TIMEOUT 30
  )
endif()
