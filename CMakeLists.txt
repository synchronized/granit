# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

cmake_minimum_required(VERSION 3.23)

project(
  granit
  VERSION 0.19.0
  DESCRIPTION "Vulkan-based C++20 rendering library"
  LANGUAGES C CXX
)

option(BUILD_SHARED_LIBS "构建共享库" ON)
option(GRANIT_BUILD_TESTING "构建 Granit 测试" ON)
option(GRANIT_BUILD_EXAMPLES "构建 Granit 示例" ${PROJECT_IS_TOP_LEVEL})
option(GRANIT_BUILD_MODEL_VIEWER_EXAMPLE "构建跨后端模型查看器示例" OFF)
option(GRANIT_FETCH_EXAMPLE_GLTF_DEPENDENCIES "下载模型查看器锁定的 glTF 私有依赖" OFF)
option(GRANIT_BUILD_BENCHMARKS "构建 Granit benchmark 程序" OFF)
option(GRANIT_BUILD_TOOLS "构建 Granit 离线工具" OFF)
option(GRANIT_BUILD_SHADER_TOOLS "构建并安装 ShaderTools SDK" OFF)
option(GRANIT_BUILD_INTEGRATION_SDL3 "构建 SDL3 可选集成组件" OFF)
option(GRANIT_BUILD_INTEGRATION_IMGUI "构建 ImGui 可选集成组件" OFF)
option(GRANIT_BUILD_EMSCRIPTEN_PLATFORM "构建 S-10D Emscripten 平台验证目标" OFF)
option(GRANIT_BUILD_WEB_IMGUI_EXAMPLE "构建浏览器 SDL3 + ImGui 示例" ON)
option(GRANIT_FETCH_INTEGRATION_DEPENDENCIES "下载锁定版本的 SDL3 与 ImGui 集成依赖" OFF)
option(GRANIT_ENABLE_XCB "在 Linux 上启用 XCB Surface 与 Window 后端" ON)
option(GRANIT_ENABLE_WAYLAND "在 Linux 上启用 Wayland Surface 与 Window 后端" ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(utility)
include(granit_integration_dependencies)

include(granit_platform)
include(CTest)

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
add_subdirectory(src)

if(NOT EMSCRIPTEN AND
   (GRANIT_BUILD_TOOLS OR GRANIT_BUILD_SHADER_TOOLS OR GRANIT_BUILD_EXAMPLES OR
    GRANIT_BUILD_BENCHMARKS OR (GRANIT_BUILD_TESTING AND BUILD_TESTING)))
  add_subdirectory(tools)
  if((GRANIT_BUILD_TESTING AND BUILD_TESTING) OR GRANIT_BUILD_BENCHMARKS)
    include(granit_shader_assets)
    granit_prepare_test_shader_assets()
  endif()
endif()

if(GRANIT_BUILD_EXAMPLES OR (EMSCRIPTEN AND GRANIT_BUILD_TESTING AND BUILD_TESTING))
  add_subdirectory(examples)
endif()
if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
  add_subdirectory(tests)
endif()
if(NOT EMSCRIPTEN)
  if(GRANIT_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
  endif()
endif()
include(granit_package)
