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

## 标准 PBR Schema

`<granit/pipeline/pbr_material.h>` 和对应 C++20 头公开标准 PBR Shader 的稳定消费契约，包括：

- 基础颜色、金属度、感知粗糙度、法线、遮蔽、发光和调试显示的参数名与常量偏移；
- 五类纹理、Sampler 的 Binding，以及 `pbr_texture_mask` 特性位；
- Position、Normal、Tangent 和 UV0 的标准 Vertex Location；
- `granit_pbr_validate_vertex_layout`，用于在创建资产前检查布局是否满足所选纹理变体。

这些值是公共标准 PBR 模板和内置实现共同使用的权威定义。上游仍拥有自己的材质语义和资产身份，
只需在 GPU 实例边界映射到该 Schema；不需要包含 `src/material` 私有头文件。

RenderPipeline 资产根目录下的 `materials/pbr_standard.grmat` 是对应的标准材质模板。其模板版本由
`GRANIT_PBR_MATERIAL_TEMPLATE_VERSION` 标识，归档内容身份由
`GRANIT_PBR_MATERIAL_CONTENT_HASH_HEX` 固定；Shader 引用与同一资产根目录中的标准 PBR Shader
配套发布。项目内权威源是 `assets/materials/pbr_standard.grmat.json`，Model Viewer 也消费该模板。

## 创建与更新

- 使用 `GRANIT_MATERIAL_DESC_INIT` 初始化创建描述。
- `archive_data` 及其长度描述材质归档；数据只需在创建调用期间有效。
- v4 归档只保存 Shader Asset 内容 ID。`shader_resolver` 在首次需要对应 Pipeline 时同步取得
  `.grshader` 清单和当前后端 sidecar；缺少解析器或依赖返回 `GRANIT_ERROR_NOT_READY`。
- `initial_updates` 在创建时整体应用；任何一步失败都不会产生 Material 句柄。
- `granit_material_update` 批量更新参数。整批更新具有事务性：失败时保留原状态。
- 空更新批次合法，可用于显式刷新或保持统一调用路径。

参数 ID 必须来自同一材质布局，类型和数据尺寸必须与归档元数据一致。Texture View 和 Sampler
必须属于同一 Renderer，并在 Material 使用期间保持有效。

## 所有权与生命周期

- Material 拥有自身的参数状态和 GPU 实例。
- Material 不拥有更新中引用的 Texture View 或 Sampler。
- Material 不拥有 resolver 返回的字节。字节只借用至一次 Shader 创建结束，但 resolver 的
  `user_data` 和资产存储必须存活到 Material 销毁，因为 Pipeline 按需延迟创建。
- 销毁后句柄立即失效；重复销毁、跨 Renderer 使用或更新旧句柄返回无效句柄错误。

## 线程安全

不同 Material 可以并发更新。同一 Material 的更新不能彼此并发，也不能与销毁并发。渲染正在
读取 Material 时，不应更新或销毁该 Material。

resolver 由 Granit 同步调用且不会重入。若资产存储可能同时写入，调用方必须提供读取同步。
