# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 前七章使用桌面窗口循环；浏览器事件循环在第八章统一接入。
if(NOT EMSCRIPTEN)
  add_subdirectory(01_window)
endif()
