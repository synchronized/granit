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
- 文档链接检查通过。

## 发布状态

**本地验证完成，未发布。** Linux/远端发布候选 CI、版本收口和公开制品复核仍未完成；不得据此创建
v0.73.0 tag 或 GitHub Release。
