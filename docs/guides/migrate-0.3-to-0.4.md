<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.3 迁移到 0.4

## 适用范围

本文记录 0.4.0 开发周期内已经发生的公共接口迁移。0.4.0 尚未发布，后续破坏性变化将继续追加到
本指南；0.3.0 使用者升级后必须重新编译。

## Bind Group 绑定描述

Graphics 与 Compute 的 Bind Group 绑定函数不再分别接收起始组、数组指针和数量，而是统一接收
`granit_bind_groups_desc`。该结构为后续 Dynamic Uniform Buffer Offset 提供可扩展边界。

旧写法：

```c
granit_command_recorder_bind_graphics_groups(
    renderer, recorder, pipeline_layout, 0, groups, group_count);
```

0.4 写法：

```c
granit_bind_groups_desc bind_desc = GRANIT_BIND_GROUPS_DESC_INIT;
bind_desc.first_group = 0;
bind_desc.bind_groups = groups;
bind_desc.bind_group_count = group_count;

granit_command_recorder_bind_graphics_groups(
    renderer, recorder, pipeline_layout, &bind_desc);
```

Compute 使用相同描述结构。C++20 包装仍可按原来的参数顺序调用，并新增可选的动态 Offset
`std::span`，普通 Bind Group 调用通常无需修改。

## Dynamic Uniform Buffer 阶段状态

`GRANIT_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER` 和动态 Offset 数组已经进入公共契约。D-10B/C 完成
布局验证及后端绑定前，创建动态 Layout 或传入非空动态 Offset 会返回
`GRANIT_ERROR_UNSUPPORTED`，调用方不得把当前阶段视为运行时能力已经可用。
