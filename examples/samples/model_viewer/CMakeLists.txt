# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# Model Viewer 的 Core、桌面壳层和 Web 壳层由示例自身统一编排。
include("${CMAKE_CURRENT_LIST_DIR}/core.cmake")
if(NOT EMSCRIPTEN)
  add_subdirectory(desktop)
endif()
add_subdirectory(
  "${CMAKE_CURRENT_LIST_DIR}/web"
  "${CMAKE_CURRENT_BINARY_DIR}/web-support"
)
