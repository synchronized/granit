# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# Model Viewer 的 Core、桌面壳层和 Web 壳层由 Sample 目标统一编排。
include("${CMAKE_CURRENT_LIST_DIR}/core.cmake")
if(GRANIT_HAS_NATIVE_WINDOW)
  add_subdirectory(desktop)
else()
  add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/web"
    "${CMAKE_CURRENT_BINARY_DIR}/web-support"
  )
endif()
