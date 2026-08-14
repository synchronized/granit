<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Render Pipeline

Render Pipeline 是可选的高级参考渲染入口。当前实现组织 Directional Shadow、Forward PBR HDR
和 ACES Tone Mapping，并输出 LDR 图像；它不是 Deferred 或 Forward+ Renderer。

## 公共入口

- C：`<granit/pipeline/render_pipeline.h>`。
- C++20：`<granit/pipeline/render_pipeline.hpp>`，使用 move-only 的
  `granit::render_pipeline`。
- 所属 CMake component：`RenderPipeline`，目标为 `granit::render_pipeline`。

创建描述使用 `GRANIT_RENDER_PIPELINE_DESC_INIT` 初始化。未提供录制回调时使用完整自动路径；
提供回调时可以覆盖 Shadow 和 Opaque 阶段的 Draw 录制。

## 每帧输入

渲染描述使用 `GRANIT_RENDER_PIPELINE_RENDER_DESC_INIT` 初始化，主要包含：

- Scene Snapshot 和要渲染的连续 View 范围。
- `payload` 到 Mesh、Material 的 Draw Binding。
- 每个 View 的输出 Texture View、格式和尺寸。
- 曝光值，以及可选的 Swapchain Frame。

每个可见 Renderable 的 `payload` 必须唯一映射到一个 Draw Binding。绑定中的 Mesh、Material、
Scene Snapshot 和输出资源必须属于同一 Renderer，并在调用期间保持有效。

单 View 可以使用紧凑的 `output`、`output_format`、`width` 和 `height` 字段。多 View 必须提供
与 `view_count` 等长的 `outputs` 数组。非零 Frame 表示录制并提交窗口帧，此时只允许一个 View；
零 Frame 表示离屏执行。

## 录制回调

回调按阶段接收 Recorder、当前 View、可见 Renderable、Draw Binding、光照输入和临时资源。
这些指针和视图只在回调期间有效。回调不得：

- 保存临时数组或回调上下文地址。
- 结束、提交或销毁传入的 Recorder。
- 递归调用同一个 Render Pipeline。

回调返回的首个错误会终止当前渲染调用，未完成的 Recorder 不会被提交。

## 生命周期与线程安全

- Pipeline 借用 Renderer，并拥有默认 IBL、内建 Shader 和跨帧缓存。
- Pipeline 不拥有 Scene Snapshot、Mesh、Material、输出目标或 Frame。
- 同一个 Pipeline 不支持并发 `render`；发生竞争时返回 `GRANIT_ERROR_NOT_READY`。
- 不要让 Pipeline、Scene、Mesh、Material 或相关资源的销毁与 `render` 并发。
- 创建时提供的回调和 `user_data` 必须保持有效，直到 Pipeline 被销毁。

当前离屏 `render` 在返回前等待本次 Recorder 完成，因此内部缓存可以安全原地更新。该行为适合
首版同步门面，但不代表未来多帧在途接口的同步承诺；引入异步提交时需要按 Frame 生命周期管理
Upload 环形分配。

## 当前范围与限制

- 渲染路径固定为 Opaque Forward PBR、可选单方向光阴影和 ACES Tone Mapping。
- 阴影目标固定为 1024×1024；尚不支持 CSM、多阴影光源或可配置阴影质量。
- 不包含透明 PBR、Unlit、Sprite、UI、Bindless、Clustered Forward 或 Deferred。
- 默认 IBL 由 Pipeline 内部持有；外部环境切换尚未进入公共接口。
- 同一 Pipeline 不支持并发渲染，多 View 仍按独立输出顺序执行。

更底层的自定义渲染流程可直接使用 [Command Recorder](command-recorder.md) 和
[Graphics Pipeline](pipeline.md)，无需经过本参考管线。
