<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-04 S-18A Gneiss 接入基线

## 结论

Gneiss 已经通过独立 Platform Service 和 Render Service 完成 Granit 的真实宿主闭环，不需要 Granit
接管其 Scene、ECS、RID 或资产格式。现有 Granit API 已覆盖 Renderer/Surface/Swapchain 创建、窗口
与输入、动态 Uniform、纹理和几何上传、UI/Debug Draw、Resize、提交、呈现及关闭资源统计。

首个确认的通用能力缺口是窗口创建后无法主动查询当前内容缩放。Granit 已提供带水平、垂直缩放和
Framebuffer 尺寸的 `GRANIT_WINDOW_EVENT_SCALE_CHANGED`，但没有对应的当前状态查询；Gneiss 因此
只能把 ImGui 初始 `DisplayFramebufferScale` 固定为 1。

## 审查范围

- Gneiss 仓库：`D:/sunday/workspace/build/github/synchronized/gneiss`
- Gneiss 分支：`feat/ver022-asset-hot-reload`
- Granit 依赖：Package、Fetch 或父工程既有目标，最低版本 0.4，Fetch 锁定完整提交。
- 审查方式：只读检查依赖解析、Application、Platform Service、Render Service、测试与现有计划。

## 已覆盖的接入路径

| 路径 | Gneiss 当前做法 | Granit 能力结论 |
|---|---|---|
| 平台创建 | Window System、Window、Input System | 已满足 |
| Surface | 按 Win32、XCB、Wayland 原生值创建 | 已满足 |
| 帧循环 | Acquire、Frame Context、Recorder、Submit、Present | 已满足 |
| Resize | 消费尺寸/缩放事件并重建 Swapchain 与深度目标 | 已满足 |
| 几何 | Gneiss RID 镜像到静态 Vertex/Index Arena | 已满足，资产仍归 Gneiss |
| 材质纹理 | 按 RID 缓存 Texture、View、Sampler、Bind Group | 已满足 |
| 对象数据 | 查询设备对齐并使用三槽 Dynamic Uniform Arena | 已满足 |
| 工具绘制 | Canvas Draw List 与 Debug Draw List 同帧录制 | 已满足 |
| 关闭 | 逆序销毁并查询 `renderer_resource_stats` | 已满足 |

## 问题分类

### Granit 通用缺口

- 增加后端无关的窗口当前状态查询，至少返回逻辑尺寸、Framebuffer 尺寸与水平/垂直内容缩放。
- 查询结果应与 `GRANIT_WINDOW_EVENT_RESIZED` 和 `GRANIT_WINDOW_EVENT_SCALE_CHANGED` 使用同一语义，
  让宿主在收到事件前也能正确初始化 UI 缩放。

### 上游迁移事项

- Gneiss 当前仍使用 `granit::succeeded(result)` 与 `granit::failed(result)` 自由函数。升级 Granit
  0.5.0 时应迁移到 `result.ok()`、`result.failed()` 或显式布尔上下文；这不是新的 Granit 能力。
- Gneiss 当前最低 Package 版本和 Fetch 提交仍指向 0.4 系列。只有 Granit 0.5.0 接入验收通过后，
  才应由 Gneiss 独立更新依赖身份和迁移文档。

### 暂无 Granit 缺口

- 资源热重载尚未证明需要新的 Granit API。Gneiss 已能创建替代 GPU 镜像，并由 Granit 现有资源
  生命周期与延迟回收保证已提交工作安全；如 M-148 得到反例，再建立最小复现。
- Docking、UI 合成、动态 Uniform、资源统计、Indexed Draw 和普通 Resize 已有完整调用路径。
- glTF 导入、Mesh Binary、资源租约、Scene Tree、ECS 和编辑器协议继续属于 Gneiss。

## 后续顺序

1. S-18B1 设计并实现窗口当前状态查询，覆盖 Win32、XCB、Wayland 的初始值和变化后一致性。
2. 建立独立 Window C/C++ Consumer，证明查询不要求 Renderer、Input 或平台私有头。
3. 补充 0.4 到 0.5 的 C++ `result` 迁移说明，再由 Gneiss 单独升级并运行 Package/Fetch 矩阵。
4. S-18C 与 S-18D 保持需求门控；资源热重载或诊断出现可复现缺口后再扩大范围。

本记录只保存审查证据；当前实施范围以
[S-18 计划](../plans/S-18-0.5.0-platform-upstream-integration.md)为准。
