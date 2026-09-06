<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.16 迁移到 0.17

0.17.0 让浏览器 WebGPU 与 Vulkan 都支持严格非阻塞 Pipeline 预热，并增加从公共 Material
直接构造预热条目的入口。

## 使用 WebGPU 原生异步预热

先查询 `GRANIT_RENDERER_FEATURE_NON_BLOCKING_PIPELINE_WARMUP_BIT`。浏览器 WebGPU 支持该能力时，
Render/Compute Pipeline 冷创建由原生异步回调完成；应用应继续调用
`granit_renderer_process_events` 并轮询异步操作，不阻塞浏览器主线程。

提交成功后，异步操作会保留 Shader 与 Pipeline Layout。调用方可销毁原句柄，但批次仍应保留到
不再查询逐项结果为止。

## 预热材质变体

使用 `granit_material_add_pipeline_warmup` 把材质归档中的实际变体加入已有批次。描述必须提供
Pass、目标颜色/深度格式和采样数；`variant` 为零时选择该 Pass 的首个变体。材质与批次必须属于
同一 Renderer，材质至少存活到批次提交完成。

这个接口只负责生成预热条目，提交、轮询、取消和结果查询仍复用统一 Pipeline Warmup 与
Async Operation API。
