<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 12：Resource Table

本教程验证 Resource Table 实验 API 的能力查询、显式选择和回退边界。当前阶段保留原有
Descriptor Indexing 能力探针，并继续使用传统 Bind Group 绘制；后续实现 Resource Table
后将在同一入口增加真实资源槽位和 Shader 变体验证。

## 观察内容

- Vulkan 桌面是否具备 Resource Table 实验实现所需的前置能力；
- WebGPU 和不支持设备是否明确显示 fallback；
- 当前 Granit 默认路径始终是传统 Bind Group。

公共应用不需要包含 Vulkan 头文件，也不能根据能力探针结果自行假定 Resource Table 已经可用。
