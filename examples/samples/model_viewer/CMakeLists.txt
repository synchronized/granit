# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# Model Viewer 的目标编排归属于示例自身。源码迁移期间显式指定独立二进制目录，避免改变
# 既有目标名称、生成资产路径和增量构建缓存。
add_subdirectory(
  "${PROJECT_SOURCE_DIR}/examples/common/gltf"
  "${CMAKE_CURRENT_BINARY_DIR}/gltf"
)
include("${CMAKE_CURRENT_LIST_DIR}/core.cmake")
add_subdirectory(
  "${CMAKE_CURRENT_LIST_DIR}/desktop"
  "${CMAKE_CURRENT_BINARY_DIR}/desktop"
)
add_subdirectory(
  "${PROJECT_SOURCE_DIR}/examples/platform/web"
  "${CMAKE_CURRENT_BINARY_DIR}/web-support"
)
