<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# R-05：Texture 与 Texture View 生命周期

## 元数据

- 设计状态：已确认
- 实现状态：已完成
- 路线图任务：R-05
- 优先级：P0
- 前置依赖：R-01、R-02
- 后续依赖：R-07、R-09、R-10

## 已确认语义

- Texture 拥有图像存储，Texture View 是引用子资源的独立句柄。
- 首期实现单 mip、单 layer、单 sample 的 2D Texture 和完整范围 2D View。
- View 格式为 `UNDEFINED` 时继承父格式；首期只允许继承或完全相同格式。
- automatic aspect 根据颜色、深度或深度模板格式解析。
- 销毁 View 不影响 Texture；销毁 Texture 级联使全部子 View 句柄失效。
- 提供原子的 `granit_texture_create_with_default_view` 便捷入口。
- Image Layout 完全由内部管理；Texture 初始数据上传留给 R-10。

## 内部实现

普通 Texture 通过 VMA 创建并拥有 `VkImage` 与 allocation；View 拥有 `VkImageView` 并持有父
Texture 记录。内部 Texture 记录包含 owned 标志，为 R-07 的非拥有 Swapchain Image 预留路径。

句柄表校验类型、generation 和 Renderer domain。Texture 销毁先移除所有 View 句柄，再在全局
Registry 锁外依次销毁 View 和拥有的图像存储。

## 验证

- C/C++ 头文件独立编译与 move-only 检查。
- 真实设备覆盖 Texture+默认 View、独立 View、格式重解释拒绝、跨 Renderer 拒绝、重复销毁和
  父 Texture 级联失效。
- 公共 API 不暴露 Vulkan、Volk 或 VMA 类型。
