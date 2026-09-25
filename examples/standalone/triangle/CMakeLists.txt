# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

cmake_minimum_required(VERSION 3.23)
project(granit_triangle LANGUAGES CXX)

include(CTest)

option(GRANIT_TRIANGLE_REBUILD_SHADERS "使用安装的 AssetTools 从 HLSL 重建 Shader Library" OFF)
if(GRANIT_TRIANGLE_REBUILD_SHADERS)
  find_package(granit CONFIG REQUIRED COMPONENTS Window AssetTools)
  if(NOT granit_ASSET_TOOL_EXECUTABLE)
    message(FATAL_ERROR "当前 Granit 安装未包含 granit_asset_tool")
  endif()
  include("${granit_SHADER_TOOLCHAIN_MODULE}")
  include("${granit_SHADER_ASSETS_MODULE}")
  granit_find_shader_toolchain()
  if(NOT GRANIT_SHADER_TOOLCHAIN_ROOT)
    message(FATAL_ERROR "没有可用的 Granit Shader Toolchain")
  endif()
  set(shader_library "${CMAKE_CURRENT_BINARY_DIR}/generated/triangle.grshlib")
  granit_add_hlsl_shader_library(
    ALL
    NAME triangle
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/triangle.grshlib.json"
    OUTPUT "${shader_library}"
    CACHE_DIR "${CMAKE_CURRENT_BINARY_DIR}/shader-cache"
    TARGET granit_triangle_shader_library
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/triangle.hlsl"
  )
else()
  find_package(granit CONFIG REQUIRED COMPONENTS Window)
  set(shader_library "${CMAKE_CURRENT_SOURCE_DIR}/triangle.grshlib")
endif()

add_executable(granit_triangle main.cpp "${shader_library}")
target_compile_features(granit_triangle PRIVATE cxx_std_20)
target_compile_definitions(
  granit_triangle PRIVATE GRANIT_STANDALONE_TRIANGLE_SHADER_LIBRARY="${shader_library}"
)
target_link_libraries(granit_triangle PRIVATE granit::granit granit::window)

if(BUILD_TESTING)
  add_executable(granit_triangle_validate validate.cpp "${shader_library}")
  target_compile_features(granit_triangle_validate PRIVATE cxx_std_20)
  target_compile_definitions(
    granit_triangle_validate
    PRIVATE GRANIT_STANDALONE_TRIANGLE_SHADER_LIBRARY="${shader_library}"
  )
  target_link_libraries(granit_triangle_validate PRIVATE granit::granit)

  add_test(NAME granit.standalone.triangle COMMAND granit_triangle --smoke-test)
  add_test(NAME granit.standalone.triangle_pixels COMMAND granit_triangle_validate)
  set_tests_properties(
    granit.standalone.triangle granit.standalone.triangle_pixels
    PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30
  )
  if(WIN32)
    set_tests_properties(
      granit.standalone.triangle granit.standalone.triangle_pixels
      PROPERTIES ENVIRONMENT_MODIFICATION
                 "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::window>"
    )
  elseif(UNIX)
    set_tests_properties(
      granit.standalone.triangle granit.standalone.triangle_pixels
      PROPERTIES ENVIRONMENT_MODIFICATION
                 "LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::window>"
    )
  endif()
endif()
