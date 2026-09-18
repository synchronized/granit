<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-05：光照与后处理实施记录

## 结果

H-05 完成多光源、方向光阴影、IBL、HDR 中间目标、Tone Mapping 和 GPU 阶段指标的参考管线闭环，
并通过离屏、窗口、多 View、长时间生命周期和跨平台安装 Consumer 验证。Render Graph 负责阶段
依赖与资源状态，Renderer 继续只执行后端中立命令。

当前公共行为见 [Render Pipeline](../reference/render-pipeline.md)、
[Environment Map](../reference/environment-map.md)和
[RenderPipeline component 契约](../reference/render-pipeline-contract.md)。

## 已验证边界

- Forward PBR 组合方向光、点光、聚光和确定性可见光列表。
- 单方向光 Shadow Pass、环境漫反射与预过滤镜面反射进入同一 Lighting 绑定布局。
- HDR 场景颜色经显式曝光和 Tone Mapping 输出，UI 与 Overlay 位于场景后处理之后。
- 多 View 各自维护 Pass 和中间目标；资源缓存遵守 Renderer 与 View 生命周期。
- 测量未证明需要 Clustered Forward、CSM 或 Bindless，因此这些能力没有进入 H-05 公共范围。

## 历史与当前入口

- [H-05 计划](../plans/H-05-lighting-pipeline.md)保存目标、退出条件和完成摘要。
- [完整历史快照](history/H-05-lighting-pipeline-history.md)保存原始方案、测量和逐阶段日志，仅用于
  追溯。

新的光照能力必须根据路线图中的重新评估条件建立独立计划。
