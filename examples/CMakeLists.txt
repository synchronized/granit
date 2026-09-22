# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set_property(DIRECTORY APPEND PROPERTY LABELS integration)

# add_subdirectory(samples/triangle)

# 示例共享基础设施（common/），随后是各示例（samples/）。
# common 是仓库私有实现，不安装、不导出。

add_subdirectory(common/gltf)
add_subdirectory(common/validation)
add_subdirectory(common/web)
add_subdirectory(common/imgui)

add_subdirectory(samples)
add_subdirectory(tutorials)
