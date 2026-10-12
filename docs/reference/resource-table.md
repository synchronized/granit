<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Resource Table（实验性）

Resource Table 是 0.59.0 的实验性资源索引契约，用于把 Texture View、Sampler 和 Storage Buffer
放入按类型划分的槽位表。它不是公共 Vulkan Descriptor 接口，也不等同于所有设备都支持的
Bindless。

## 当前状态

当前阶段已提供 C/C++ 句柄、能力查询、槽位更新和生命周期校验。支持 Descriptor Indexing 的
Vulkan 后端会报告 `NATIVE` 并更新真实 descriptor；`LOGICAL` 只表示 CPU 侧契约可用。Pipeline
Layout、Command Recorder 绑定和 Shader 读取仍在实施中，完成前不得把该 API 用作生产渲染路径。

## 使用边界

- 使用 `granit_renderer_get_resource_table_capabilities` 查询能力；
- `AUTO` 允许后端选择实验路径或回退，`EXPLICIT` 在后端不支持时返回
  `GRANIT_ERROR_UNSUPPORTED`；
- 槽位资源必须属于创建该表的 Renderer；
- 句柄类型必须与槽位类型一致；
- Storage Buffer 更新必须提供非零范围，`GRANIT_WHOLE_SIZE` 表示从 offset 到 Buffer 末尾；
- 批量更新先完成全部校验，任一更新失败时不会部分写入；
- Resource Table 销毁后句柄立即失效；Renderer 销毁会使其所属表全部失效。

## 默认路径

Bind Group 仍是 Granit 的默认绑定路径。现有 Material、Model Viewer 和 Tutorial 01～11 不依赖
Resource Table；Tutorial 12 用于展示能力查询和后续实验路径。

公共头文件为 `granit/renderer/resource_table.h` 和 `granit/renderer/resource_table.hpp`。
