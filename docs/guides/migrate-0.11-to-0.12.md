<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.11 迁移到 0.12

0.12.0 公共化 Shader Asset 元数据检查、标准 PBR Schema/Material，并统一三种 CMake 消费模式的
RenderPipeline 资产目录。既有 Shader 与 Material 持久化格式没有变化。

## 必须操作

1. 重新编译应用及使用 Granit C/C++ 头文件的模块。
2. 将 `find_package(granit 0.11 ...)` 更新为 `find_package(granit 0.12 ...)`。
3. 删除通过固定偏移读取 `.grshader` 的代码，改用 `granit_shader_asset_inspect` 或
   `granit::inspect_shader_asset`。
4. 若复制过 Model Viewer 的 PBR 参数、Binding 或材质模板，改用
   `<granit/pipeline/pbr_material.h>` 和
   `${granit_RENDER_PIPELINE_ASSET_DIR}/materials/pbr_standard.grmat`。

## 构建树消费

`find_package`、`FetchContent_MakeAvailable(granit)` 与 `add_subdirectory(granit)` 现在均提供
`granit_RENDER_PIPELINE_ASSET_DIR`。不要再拼接 Granit 源码目录；应用仍自行决定读取、打包或异步
加载这些资产。

## 无需操作

- 无需重新编译 `.grshader` 或 `.grmat`；格式版本未变化。
- 无需把 Gneiss 等上游的产品材质语义迁入 Granit；仅在 GPU 材质实例边界映射标准 Schema。
