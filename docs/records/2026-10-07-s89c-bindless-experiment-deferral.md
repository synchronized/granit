<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-89C：Vulkan Bindless 实验延期记录

## 决策

本版本不实现实际 Vulkan Descriptor 数组采样。现有 Granit 已能查询设备是否具备 Descriptor
Indexing 前置能力，但公共层尚未定义资源索引写入、Shader/Pipeline 变体选择和 GPU 延迟回收的
完整契约。

## 原因

直接在教程或公共示例中包含 Vulkan 类型会违反 Granit 的封装边界；只在内部创建 Descriptor
数组又无法形成可复用的用户路径和有效的传统路径对照。因此 S-89C 延期，避免把设备能力误报为
Granit Bindless 能力。

## 后续准入条件

- Resource Table 索引写入和资源生命周期契约稳定；
- Shader/Pipeline 变体能显式选择 Bindless，不静默改变布局；
- 传统 Bind Group 回退和跨后端能力语义有对应测试；
- 真实场景能够测量 CPU、绑定切换、内存和 GPU 收益。
