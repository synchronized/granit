<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 12：Resource Table Scene

本教程验证 Resource Table 实验 API 的能力查询、自动模式、资源槽位更新和回退边界，并显示
仓库内置的 Suzanne glTF 多纹理材质。教程会创建一个同时包含 sampled texture view 和 sampler
的表并填充 0 号槽位；可通过 `--stress-test` 创建 64 个表并重复更新槽位，用于观察资源容量
和生命周期压力。模型当前仍通过默认 Bind Group 绘制，Resource Table 作为显式资源索引探针，
不会改变默认渲染路径。

## 观察内容

- Vulkan 桌面是否具备 Resource Table 实验实现所需的前置能力；
- WebGPU 和不支持设备是否明确显示 fallback；
- Sampler 槽位更新是否成功；
- Suzanne 的多纹理材质是否能完成资产加载、上传和可见绘制；
- Base Color texture view 与 sampler 是否能同时写入 Resource Table 0 号槽位；
- `--stress-test` 是否能稳定创建和更新 64 个表；
- 当前 Granit 默认路径始终是传统 Bind Group。

公共应用不需要包含 Vulkan 头文件，也不能根据能力探针结果自行假定 Resource Table 已经可用。
