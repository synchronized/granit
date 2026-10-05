# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(gen "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(snapshot "${CMAKE_CURRENT_SOURCE_DIR}/transparency.grshlib")
set(include_file "${gen}/transparency.grshlib.inc")
if(EMSCRIPTEN)
  set(library "${snapshot}")
else()
  set(library "${gen}/transparency.grshlib")
  granit_add_hlsl_shader_library(NAME tutorial_10_transparency
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/transparency.grshlib.json" OUTPUT "${library}"
    CACHE_DIR "${gen}/cache" REFERENCE "${snapshot}" TARGET granit_tutorial_10_shader_library
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/transparency.hlsl")
endif()
add_custom_command(OUTPUT "${include_file}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${gen}"
  COMMAND "${CMAKE_COMMAND}" "-DINPUT=${library}" "-DOUTPUT=${include_file}"
          -P "${PROJECT_SOURCE_DIR}/cmake/assets/embed_binary.cmake"
  DEPENDS "${library}" "${PROJECT_SOURCE_DIR}/cmake/assets/embed_binary.cmake"
  COMMENT "内嵌 10 Transparency Shader Library" VERBATIM)
add_executable(granit_tutorial_10_transparency main.cpp shader_archive.cpp shader_archive.h "${include_file}")
if(NOT EMSCRIPTEN)
  add_dependencies(granit_tutorial_10_transparency granit_tutorial_10_shader_library)
endif()
target_include_directories(granit_tutorial_10_transparency PRIVATE "${gen}")
target_compile_features(granit_tutorial_10_transparency PRIVATE cxx_std_20)
target_link_libraries(granit_tutorial_10_transparency PRIVATE granit_example_application granit_example_tutorial granit::render_pipeline)
granit_target_compile_warnings(granit_tutorial_10_transparency)
granit_target_output_directories(granit_tutorial_10_transparency)
if(EMSCRIPTEN)
  include(granit_web)
  granit_target_webgpu(granit_tutorial_10_transparency)
  granit_target_web_page(granit_tutorial_10_transparency "${CMAKE_CURRENT_LIST_DIR}/web_shell.html")
elseif(BUILD_TESTING)
  add_test(NAME granit.tutorial.10_transparency COMMAND granit_tutorial_10_transparency --smoke-test)
  set_tests_properties(granit.tutorial.10_transparency PROPERTIES LABELS "tutorial;smoke;gpu;integration;platform"
    SKIP_RETURN_CODE 77 FAIL_REGULAR_EXPRESSION "ImGui Draw Data|销毁时仍有" TIMEOUT 30)
endif()
