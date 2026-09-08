# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "check_backend_native_boundaries.cmake 需要 SOURCE_DIR")
endif()

file(
  GLOB_RECURSE source_files
  LIST_DIRECTORIES FALSE
  "${SOURCE_DIR}/src/*.c"
  "${SOURCE_DIR}/src/*.cc"
  "${SOURCE_DIR}/src/*.cpp"
  "${SOURCE_DIR}/src/*.h"
  "${SOURCE_DIR}/src/*.hpp"
)

foreach(source_file IN LISTS source_files)
  file(TO_CMAKE_PATH "${source_file}" normalized_file)
  if(normalized_file MATCHES "/src/backend/")
    continue()
  endif()
  file(READ "${source_file}" content)
  if(content MATCHES "(^|[^A-Za-z0-9_])(WGPU[A-Za-z0-9_]*|wgpu[A-Za-z0-9_]+)")
    message(FATAL_ERROR "WebGPU 原生符号越过 Backend 边界：${source_file}")
  endif()
  if(normalized_file MATCHES "/src/renderer/" AND
     content MATCHES "(^|[^A-Za-z0-9_])(Vk[A-Z][A-Za-z0-9_]*|VK_[A-Z0-9_]+)")
    message(FATAL_ERROR "Vulkan 原生符号越过 Renderer/Backend 边界：${source_file}")
  endif()
endforeach()

# 桌面核心目标只允许编译 Vulkan；浏览器 WebGPU 源码由 src/emscripten.cmake 独立选择。
file(READ "${SOURCE_DIR}/src/CMakeLists.txt" desktop_sources)
if(desktop_sources MATCHES "backend/webgpu|backend/plugin|webgpu_provider|backend_plugin")
  message(FATAL_ERROR "桌面核心目标重新引入了 WebGPU Provider 或插件边界")
endif()

file(READ "${SOURCE_DIR}/src/emscripten.cmake" browser_sources)
# 浏览器目标通过共享清单选择后端，检查实际清单而非要求入口重复列出源文件。
set(PROJECT_SOURCE_DIR "${SOURCE_DIR}")
include("${SOURCE_DIR}/cmake/granit_core_sources.cmake")
set(webgpu_factory "${SOURCE_DIR}/src/backend/webgpu/renderer_factory.cpp")
set(webgpu_provider "${SOURCE_DIR}/src/backend/webgpu/provider.cpp")
if(NOT browser_sources MATCHES "GRANIT_WEBGPU_BACKEND_SOURCES" OR
   NOT webgpu_factory IN_LIST GRANIT_WEBGPU_BACKEND_SOURCES OR
   NOT webgpu_provider IN_LIST GRANIT_WEBGPU_BACKEND_SOURCES)
  message(FATAL_ERROR "浏览器目标缺少 Emscripten WebGPU 静态实现")
endif()
