<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 12：Resource Table Scene

本教程验证 Resource Table 实验 API 的能力查询、自动模式、资源槽位更新和回退边界。教程会
创建一个自动模式的 Sampler 表并填充 0 号槽位；可通过 `--stress-test` 创建 64 个表并重复
更新槽位，用于观察资源容量和生命周期压力。当前实际绘制仍使用传统 Bind Group，避免让实验
API 改变默认渲染路径；Suzanne/小型 glTF 的可见场景接入属于 v0.60.0 后续收口项。

## 观察内容

- Vulkan 桌面是否具备 Resource Table 实验实现所需的前置能力；
- WebGPU 和不支持设备是否明确显示 fallback；
- Sampler 槽位更新是否成功；
- `--stress-test` 是否能稳定创建和更新 64 个表；
- 当前 Granit 默认路径始终是传统 Bind Group。

公共应用不需要包含 Vulkan 头文件，也不能根据能力探针结果自行假定 Resource Table 已经可用。
