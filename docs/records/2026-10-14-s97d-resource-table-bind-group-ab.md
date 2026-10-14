<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-97D：Resource Table 与 Bind Group A/B 压力对照首轮记录

## 日期与范围

- 日期：2026-10-14
- 计划：[S-97](../plans/S-97-0.61.0-resource-table-shader-and-multimaterial-scene.md)
- 环境：Windows Clang Debug、Vulkan 后端、当前默认设备
- 负载：Tutorial 12，16 个材质、16 个 Draw

## 验证方式

Tutorial 12 增加两个压力入口：

```powershell
build/windows-clang-debug/bin/granit_tutorial_12_resource_table_scene.exe --stress-test
build/windows-clang-debug/bin/granit_tutorial_12_resource_table_scene.exe --bind-group-stress-test
```

两条路径使用同一份模型、纹理和绘制负载。Resource Table 路径额外创建 64 个压力表并执行
sampler 更新；Bind Group 路径不创建 Resource Table，使用传统 Material/Bind Group implementation。
两条路径均启用已有 RenderPipeline GPU timestamp 指标，输出场景初始化 CPU 时间和 Shadow、Opaque、
Tone Mapping、Total GPU 时间。

## 首轮结果

| 路径 | 场景初始化 | Resource Table 更新 | Total GPU |
|---|---:|---:|---:|
| Resource Table | 117178 μs | 82 | 6.500890 ms |
| Bind Group | 139225 μs | 0 | 6.662223 ms |

Resource Table 压力入口另报告 64 个压力表和 64 次 sampler 更新；两条路径均正常退出，GPU 指标
均成功返回。当前结果只是单机、单负载、Debug 构建的首轮观测，不能据此宣布 Resource Table
在所有设备上更快。

## 五轮重复测量

同一 Windows Clang Debug、16 材质/16 Draw 负载下分别运行 5 次，取中位数：

| 路径 | 场景初始化 p50 | Total GPU p50 |
|---|---:|---:|
| Resource Table | 117024 μs | 6.384 ms |
| Bind Group | 115629 μs | 6.486 ms |

在这组样本中 Resource Table 的 GPU p50 约快 1.6%，但 CPU 初始化 p50 约慢 1.2%。差异很小，且
Resource Table 仍包含额外的 64 个压力表创建和更新，因此这只能作为当前设备上的观测，不能作为
跨设备或产品默认路径的准入依据。

## 限制与后续

- 当前 Bind Group 路径输出场景材质数量作为对照规模，尚未接入后端级创建/Descriptor 写入计数器；
- 尚未完成 4/16/32 材质和 8/32/64 Draw 的矩阵化压力报告；
- 还需要 Linux Lavapipe、Release 构建和 WebGPU Bind Group 回退数据；
- GPU timestamp 不可用时只保留 CPU 和资源统计，并明确标记 GPU 数据 unavailable。
