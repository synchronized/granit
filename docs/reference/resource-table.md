<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Resource Table（实验性）

Resource Table 是 0.59.0 的实验性资源索引契约，用于把 Texture View、Sampler 和 Storage Buffer
放入按类型划分的槽位表。它不是公共 Vulkan Descriptor 接口，也不等同于所有设备都支持的
Bindless。

## 当前状态

当前阶段已提供 C/C++ 句柄、能力查询、槽位更新、Pipeline Layout 声明和 Command Recorder
绑定。支持 Descriptor Indexing 的 Vulkan 后端会报告 `NATIVE` 并更新真实 descriptor；不具备
原生表的后端报告 `FALLBACK`，自动模式仍可维护逻辑槽位。Shader 读取变体和 Material 接入尚未
纳入本版本，不能把该 API 用作生产渲染路径。

## 使用边界

- 使用 `granit_renderer_get_resource_table_capabilities` 查询能力；
- `AUTO` 允许后端选择实验路径或回退，`EXPLICIT` 在后端不支持时返回
  `GRANIT_ERROR_UNSUPPORTED`；
- 槽位资源必须属于创建该表的 Renderer；
- 句柄类型必须与槽位类型一致；
- Storage Buffer 更新必须提供非零范围，`GRANIT_WHOLE_SIZE` 表示从 offset 到 Buffer 末尾；
- 批量更新先完成全部校验，任一更新失败时不会部分写入；
- Resource Table 销毁后句柄立即失效；Renderer 销毁会使其所属表全部失效。
- Resource Table 必须在对应 Pipeline Layout 中声明，并绑定到该布局最后一个 descriptor set；
  不匹配的布局或表会返回错误。

## 默认路径

Bind Group 仍是 Granit 的默认绑定路径。现有 Material、Model Viewer 和 Tutorial 01～11 不依赖
Resource Table；Tutorial 12 用于展示能力查询和后续实验路径。

公共头文件为 `granit/renderer/resource_table.h` 和 `granit/renderer/resource_table.hpp`。
