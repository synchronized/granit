<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-03：PBR 渲染模块实施记录

## 结果

H-03 完成金属度/粗糙度 PBR 的首轮参考实现，验证材质包、顶点输入、深度状态、显式 Draw、
Render Graph 适配和 GPU 像素回归可以组成稳定离屏闭环。模块保持在可选高层，不改变核心 Renderer
的后端中立定位。

当前 PBR 已由 RenderPipeline 组合 Shadow、IBL、Tone Mapping、Scene Snapshot 和质量配置。使用者
应以 [Render Pipeline](../reference/render-pipeline.md)、[Material](../reference/material.md)和
[Mesh](../reference/mesh.md)为准。

## 已验证边界

- 首轮路径采用 Forward PBR，不建立 G-Buffer 或 Deferred Lighting Pass。
- 材质与 Object/Frame 数据按稳定绑定频率分组，资源归属和生命周期在录制前校验。
- CPU BRDF 数值参考、默认纹理和 GPU 像素结果共同覆盖材质退化路径。
- PBR 是可替换参考模块；调用方仍可直接使用 Pipeline、Bind Group 和 Command Recorder。
- Scene、可见性、多光源、阴影和后处理由后续高层任务组合，不进入核心 Renderer。

## 历史与当前入口

- [H-03 计划](../plans/H-03-pbr-renderer.md)保存目标和完成差异。
- [完整历史快照](history/H-03-pbr-renderer-history.md)保存早期边界与逐阶段日志，仅用于追溯。

后续画质或 Pipeline 变化应更新当前 Reference 或建立独立计划。
