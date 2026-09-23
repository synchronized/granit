# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set_property(DIRECTORY APPEND PROPERTY LABELS integration)

# 示例共享基础设施（common/），随后是综合示例（samples/）和特性教程（tutorials/）。
# common 是仓库私有实现，不安装、不导出。

add_subdirectory(common/gltf)
add_subdirectory(common/assets)
add_subdirectory(common/application)
add_subdirectory(common/model_viewer)
add_subdirectory(common/validation)
add_subdirectory(common/web)
add_subdirectory(common/imgui)

add_subdirectory(samples)
add_subdirectory(tutorials)
