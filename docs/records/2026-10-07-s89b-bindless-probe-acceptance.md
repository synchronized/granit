<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-89B：Bindless 能力探针验收记录

## 已完成

- 增加后端无关的 `GRANIT_RENDERER_FEATURE_BINDLESS_DESCRIPTOR_INDEXING_BIT` 能力位；
- Vulkan 查询并启用实验前置 Descriptor Indexing 特性，WebGPU 不报告该能力；
- 新增 `12_bindless_probe` 教程，显示设备能力、实验后端状态和传统 Bind Group 回退；
- 通过 Windows Clang shared 的教程编译与 `granit.tutorial.12_bindless_probe` Smoke 测试。

## 边界

该能力位只表示设备具备 S-89C 所需的前置特性，不表示 Granit 已经提供 Bindless 资源表或材质
变体。当前默认渲染路径没有改变，公共接口仍不暴露 Vulkan 类型或 Descriptor 绑定布局。
