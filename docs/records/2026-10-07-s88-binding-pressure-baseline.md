<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-88 真实绑定压力基线

## 范围

本记录对应 [S-88](../plans/S-88-0.52.0-binding-pressure-and-resource-table-gate.md) 的
S-88A 和 Windows Vulkan 部分 S-88B。基准使用 `pbr_textured` 片元变体，实际读取
`base_color_texture`、`metallic_roughness_texture`、`normal_texture`、`occlusion_texture` 和
`emissive_texture`，并使用 `pbr_sampler`；不再使用只绘制常量颜色的 `untextured` 变体。

## 运行环境

- 平台：Windows AMD64；Clang 23.1.1；shared Debug；Vulkan 后端；
- 构建目标：`granit_render_pipeline_benchmarks`；
- 参数：`iterations=1`、`samples=2`、`warmup=1`、`lights=0`、`shadow-range=near`；
- 测试点：8、64、512 draws/materials/texture groups；
- 输出元数据：`shader_sampling=pbr_textured`、`texture_bindings=5`。

## 结果摘要

| 规模 | 材质创建 P50 | 绑定组更新 P50 | 自动端到端 P50 | GPU opaque P50 |
|---:|---:|---:|---:|---:|
| 8 | 313,500 ns | 71,400 ns | 3,131,200 ns | 79,416 ns |
| 64 | 314,600 ns | 71,700 ns | 20,747,700 ns | 534,164 ns |
| 512 | 290,500 ns | 72,400 ns | 496,616,600 ns | 4,183,900 ns |

每个规模均报告了 `material_bind_groups=规模`、`texture_group_switches=规模-1`；自动路径
报告的 `pipeline_cache_entries` 为 `规模*2`。64 和 512 规模的完整 benchmark 进程均成功退出。

## 闸门结论

当前证据确认了真实纹理采样和传统 Bind Group 的容量边界，但不能把自动路径的增长直接归因于
Bind Group：材质创建与资源更新 P50 基本稳定，而自动路径同时增加了管线缓存和完整绘制成本。
因此 v0.52.0 暂不进入 Bindless 或 Resource Table 原型，继续保持传统 Bind Group 默认路径；后续
若要重新开启 D-09C，必须补充隔离管线创建、绑定组创建/更新、缓存命中和设备内存的独立测量。

## 未覆盖项

本记录尚未提供 Linux Lavapipe 的运行时数据，也未运行 Emscripten/WebGPU benchmark 行为测试；
S-88B 的跨平台门禁仍需完成，不能据此宣称全平台通过。
