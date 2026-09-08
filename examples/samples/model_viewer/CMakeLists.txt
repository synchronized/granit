# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# Model Viewer 的目标编排归属于示例自身。源码迁移期间显式指定独立二进制目录，避免改变
# 既有目标名称、生成资产路径和增量构建缓存。
include("${CMAKE_CURRENT_LIST_DIR}/core.cmake")
if(NOT EMSCRIPTEN)
  add_subdirectory(desktop)
endif()
add_subdirectory(
  "${CMAKE_CURRENT_LIST_DIR}/web"
  "${CMAKE_CURRENT_BINARY_DIR}/web-support"
)
