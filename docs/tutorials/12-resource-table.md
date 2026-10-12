<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 12：Resource Table

本教程验证 Resource Table 实验 API 的能力查询、自动模式、资源槽位更新和回退边界。教程会
创建一个自动模式的 Sampler 表并填充 0 号槽位；实际绘制仍使用传统 Bind Group，避免让实验
API 改变基础教程和默认渲染路径。

## 观察内容

- Vulkan 桌面是否具备 Resource Table 实验实现所需的前置能力；
- WebGPU 和不支持设备是否明确显示 fallback；
- Sampler 槽位更新是否成功；
- 当前 Granit 默认路径始终是传统 Bind Group。

公共应用不需要包含 Vulkan 头文件，也不能根据能力探针结果自行假定 Resource Table 已经可用。
