# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT GRANIT_HAS_NATIVE_WINDOW)
  include("${CMAKE_CURRENT_LIST_DIR}/emscripten.cmake")
  return()
endif()

# 浏览器入口直接复用 Model Viewer 的输入累积器，不保留平行输入状态机。
