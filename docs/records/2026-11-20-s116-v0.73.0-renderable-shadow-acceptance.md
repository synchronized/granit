<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-116 Renderable 阴影投射与接收验收

## 范围

验证 `granit_scene_renderable` 的实例级阴影控制：默认投射和接收、关闭投射、关闭接收，以及
未知标志拒绝。关闭投射不得影响主视图可见性；关闭接收不得影响非阴影光照。

## 实现摘要

- 在现有 `shadow_flags` 布局位置增加两个禁用位，不扩大 Renderable 元素布局；
- Scene Snapshot 和内部 Scene 提交校验未知位；
- Directional Shadow Caster 收集阶段过滤关闭投射的实例；
- PBR Object 常量传递接收状态；运行时 PBR Shader 通过专用变体控制阴影采样；
- 测试专用全屏 PBR Shader 保持原有 Descriptor Layout。

## 本地验证

- Windows Clang 增量构建通过；
- Scene、Lighting、Material、RenderPipeline 聚焦测试通过；
- 全量 CTest 通过，119/119；
- Linux 发布候选核心矩阵通过：Clang/GCC × shared/static Consumer，以及 Lavapipe Resource Table 回归；
- 文档链接检查通过。

Linux Integration smoke test 的模型查看器阶段报告 `provider-events: unsupported operation`，shared/static
均一致失败。该阶段不属于 S-116 阴影控制路径；本次发布门禁采用 Release 工作流覆盖的 Linux 核心构建、安装
Consumer、离屏示例和 Runtime Bundle 验证。

## 发布状态

**发布候选验收完成，待正式 Release 工作流。** Windows 本地与 Linux 核心发布候选验证已完成；正式
`v0.73.0` tag、GitHub Release 和公开制品复核由 Release 工作流继续执行。
