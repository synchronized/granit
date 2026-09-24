# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 示例共享的 SDL3 与 ImGui SDL 后端生命周期工具。依赖通过本目标传播，使用者不需要
# 直接拼装 SDL3、ImGui 和后端实现。

if(NOT GRANIT_SDL3_TARGET OR NOT TARGET "${GRANIT_SDL3_TARGET}")
  message(FATAL_ERROR "示例 SDL 支撑层需要可用的 SDL3 target")
endif()
if(NOT GRANIT_IMGUI_BACKEND_SDL3_TARGET OR
   NOT TARGET "${GRANIT_IMGUI_BACKEND_SDL3_TARGET}")
  message(FATAL_ERROR "示例 SDL 支撑层需要可用的 ImGui SDL3 backend target")
endif()

add_library(granit_example_sdl INTERFACE)
target_sources(
  granit_example_sdl
  INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/sdl3_lifecycle.h"
)
target_compile_features(granit_example_sdl INTERFACE cxx_std_20)
target_include_directories(
  granit_example_sdl INTERFACE "${PROJECT_SOURCE_DIR}/examples/common"
)
target_link_libraries(
  granit_example_sdl
  INTERFACE "${GRANIT_SDL3_TARGET}" "${GRANIT_IMGUI_BACKEND_SDL3_TARGET}"
)
set_target_properties(granit_example_sdl PROPERTIES FOLDER "Examples/Common")
