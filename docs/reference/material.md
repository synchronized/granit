<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Material

Material 实例由材质归档创建，保存已验证的参数布局、GPU 绑定和当前参数值。它属于高级
Render Pipeline component，不取代核心 Renderer 的 Shader、Pipeline 或 Bind Group 接口。

## 公共入口

- C：`<granit/pipeline/material.h>`。
- C++20：`<granit/pipeline/material.hpp>`，使用 move-only 的 `granit::material_instance`。
- 所属 CMake component：`RenderPipeline`，目标为 `granit::render_pipeline`。

`granit_material_parameter_id` 根据参数名生成稳定 ID。C++ 用户可以使用
`granit::material_parameter_id`。参数更新支持标量、向量、矩阵、Texture View 和 Sampler。

## 创建与更新

- 使用 `GRANIT_MATERIAL_DESC_INIT` 初始化创建描述。
- `archive_data` 及其长度描述材质归档；数据只需在创建调用期间有效。
- `initial_updates` 在创建时整体应用；任何一步失败都不会产生 Material 句柄。
- `granit_material_update` 批量更新参数。整批更新具有事务性：失败时保留原状态。
- 空更新批次合法，可用于显式刷新或保持统一调用路径。

参数 ID 必须来自同一材质布局，类型和数据尺寸必须与归档元数据一致。Texture View 和 Sampler
必须属于同一 Renderer，并在 Material 使用期间保持有效。

## 所有权与生命周期

- Material 拥有自身的参数状态和 GPU 实例。
- Material 不拥有更新中引用的 Texture View 或 Sampler。
- 销毁后句柄立即失效；重复销毁、跨 Renderer 使用或更新旧句柄返回无效句柄错误。

## 线程安全

不同 Material 可以并发更新。同一 Material 的更新不能彼此并发，也不能与销毁并发。渲染正在
读取 Material 时，不应更新或销毁该 Material。

