<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# R-07：Swapchain Backbuffer 资源接入

## 元数据

- 设计状态：已确认
- 实现状态：已完成
- 路线图任务：R-07
- 优先级：P0
- 前置依赖：R-05
- 后续依赖：R-09、F-06

## 已确认语义

- 每张 Swapchain Image 注册为非拥有 Texture，并创建完整范围默认 View。
- `granit_swapchain_get_backbuffer` 按稳定索引返回借用句柄。
- 借用 Texture/View 由 Swapchain 管理，用户显式销毁返回不支持。
- 重建、销毁 Swapchain 或销毁父 Surface 会使全部旧 Backbuffer 句柄失效。
- Backbuffer Texture 不包含 VMA allocation，绝不销毁 Swapchain 拥有的 `VkImage`。
- 重建前先销毁旧 Image View，再销毁旧 Swapchain，满足 Vulkan 生命周期要求。
- acquire/present 和“当前图像索引”仍由 F-06 实现。

## 验证

真实 Win32 测试覆盖按索引查询、越界、拒绝显式销毁、重建 generation 失效以及父级级联清理。
