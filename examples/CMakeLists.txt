# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(EMSCRIPTEN)
  # 浏览器集成测试复用模型查看器运行层，关闭示例时也只创建其私有支撑库。
  if(GRANIT_BUILD_MODEL_VIEWER_EXAMPLE OR (GRANIT_BUILD_TESTING AND BUILD_TESTING))
    add_subdirectory(common/gltf)
    add_subdirectory(common/imgui)
    add_subdirectory(common/web)
    add_subdirectory(samples/model_viewer)
  endif()
  add_subdirectory(samples/imgui)
  return()
endif()

# 示例共享基础设施（common/），随后是各示例（samples/）。
# common 是仓库私有实现，不安装、不导出。

add_subdirectory(common/gltf)
add_subdirectory(common/validation)
add_subdirectory(common/web)
add_subdirectory(common/imgui)

if(GRANIT_BUILD_MODEL_VIEWER_EXAMPLE)
  add_subdirectory(samples/model_viewer)
endif()

add_subdirectory(samples/imgui)
