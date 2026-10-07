<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 12：Bindless 能力探针

本教程只探测设备是否具备 v0.53.0 实验路径所需的 Descriptor Indexing 能力，并展示明确的回退
状态。它不会启用 Bindless 材质路径，也不会替换传统 Bind Group。

## 观察内容

- Vulkan 桌面显示设备是否具备 Descriptor Indexing 前置能力；
- WebGPU 和不支持设备明确显示 fallback；
- 当前 Granit 默认路径始终是传统 Bind Group。

Resource Table 的 CPU 生命周期原型和 Vulkan 实际索引采样分别由 S-89A/S-89C 验证。公共应用
不需要包含 Vulkan 头文件，也不能根据这个探针结果自行假定 Bindless API 已经可用。
