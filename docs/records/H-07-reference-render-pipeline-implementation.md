<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-07：高级参考渲染套件实施记录

## 结果

H-07 将 Material、Scene Snapshot、Forward PBR、Shadow、IBL、Tone Mapping、Canvas、Debug Draw 和
Text 组合为可安装的 `RenderPipeline` component，形成默认可运行的整帧门面。核心 Renderer、内部
Render Graph 和高层套件保持单向依赖，调用方仍可直接使用较低层接口。

当前使用方式和行为见 [架构分层](../concepts/architecture.md#renderer-与高级渲染套件)、
[Render Pipeline](../reference/render-pipeline.md)和
[RenderPipeline component 契约](../reference/render-pipeline-contract.md)。

## 已验证边界

- 单 View 与多 View 使用同一 Scene、Material 和 Pipeline 资源模型。
- 默认路径管理中间目标、Pass 顺序和缓存，外部目标与 Frame 生命周期仍由调用方控制。
- 空可见集合仍执行 clear、Tone Mapping、Canvas、Overlay 和 Frame 提交。
- 公共回调具有明确线程、失败和未完成 Recorder 处理语义。
- 套件作为可选安装 component 交付，不成为核心库的传递依赖。

## 历史与当前入口

- [H-07 计划](../plans/H-07-reference-render-pipeline.md)保存目标和完成差异。
- [完整历史快照](history/H-07-reference-render-pipeline-history.md)保存公共 ABI 收敛过程和逐阶段日志，
  仅用于追溯。

后续扩展应以当前 Reference 为起点，不从历史草案恢复已被替换的接口。
