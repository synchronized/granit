# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

cmake_minimum_required(VERSION 3.23)

project(
  granit
  VERSION 0.25.0
  DESCRIPTION "Multi-backend C++20 rendering library"
  LANGUAGES C CXX
)


include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(granit_options)
include(granit_platform)
include(granit_features)

include(granit_utility)
include(granit_asset_paths)
include(granit_integration_dependencies)

include(CTest)

if(GRANIT_NEEDS_SHADER_TOOLCHAIN)
  include(granit_shader_toolchain)
  granit_find_shader_toolchain()
endif()

set(
  GRANIT_INSTALL_CMAKEDIR
  "${CMAKE_INSTALL_LIBDIR}/cmake/granit"
  CACHE STRING
  "Granit CMake 包配置安装目录"
)

if(NOT EMSCRIPTEN)
  if(GRANIT_BUILD_INTEGRATION_SDL3)
    granit_prepare_sdl3_dependency(FALSE)
  endif()
  if(GRANIT_BUILD_INTEGRATION_IMGUI)
    granit_prepare_imgui_dependency(FALSE)
  endif()
  include(granit_build_assets)
endif()

add_subdirectory(3rd)
include(granit_shader_assets)
add_subdirectory(src)

if(GRANIT_NEEDS_TOOLS)
  add_subdirectory(tools)
  if(TARGET granit_asset_tool)
    granit_prepare_runtime_shader_libraries()
  endif()
  if(GRANIT_TESTING_ENABLED OR GRANIT_BUILD_BENCHMARKS)
    granit_prepare_test_shader_assets()
  endif()
endif()

if(GRANIT_BUILD_EXAMPLES OR (EMSCRIPTEN AND GRANIT_TESTING_ENABLED))
  add_subdirectory(examples)
endif()
if(GRANIT_TESTING_ENABLED)
  add_subdirectory(tests)
endif()
if(NOT EMSCRIPTEN)
  if(GRANIT_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
  endif()
endif()
include(granit_package)
