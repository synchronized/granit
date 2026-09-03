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
提供回调时可以覆盖 Shadow 和 Opaque 阶段的 Draw 录制，并在 Tone Mapping 后接收 Overlay 阶段。

Version 2 创建描述的 `sample_count` 控制自动 PBR 路径的采样数，当前接受 1 或 4。选择 4 时，
HDR 颜色和深度先写入四倍多采样附件，颜色在 PBR 阶段结束时解析到单采样 HDR 目标，再进入
Tone Mapping。自定义录制回调仍使用单采样契约，避免回调在未声明支持时收到多采样目标。

## 每帧输入

渲染描述使用 `GRANIT_RENDER_PIPELINE_RENDER_DESC_INIT` 初始化，主要包含：

- Scene Snapshot 和要渲染的连续 View 范围。
- `payload` 到 Mesh、Material 的 Draw Binding。
- 每个 View 的输出 Texture View、格式和尺寸。
- 单 View 的可选 `canvas`，或多 View 中每个输出各自的 `canvas`。
- 曝光值，以及可选的 Swapchain Frame。
- 可选的环境光纹理和采样参数。

每个可见 Renderable 的 `payload` 必须唯一映射到一个 Draw Binding。绑定中的 Mesh、Material、
Scene Snapshot 和输出资源必须属于同一 Renderer，并在调用期间保持有效。

单 View 可以使用紧凑的 `output`、`output_format`、`width` 和 `height` 字段。多 View 必须提供
与 `view_count` 等长的 `outputs` 数组。非零 Frame 表示录制并提交窗口帧，此时只允许一个 View；
零 Frame 表示离屏执行。

非零世界 Debug Draw List 会在 Tone Mapping 后复用当前 View 的深度附件录制；随后录制可选 Canvas
Draw List，最后调用用户 Overlay 回调。UNORM 输出自动启用 Shader sRGB 编码；sRGB Attachment 由
硬件完成编码。不同 View 可以使用不同 Debug Draw 和 Canvas 列表。

## 录制回调

回调按阶段接收 Recorder、当前 View、可见 Renderable、Draw Binding、光照输入和临时资源。
这些指针和视图只在回调期间有效。回调不得：

- 保存临时数组或回调上下文地址。
- 结束、提交或销毁传入的 Recorder。
- 递归调用同一个 Render Pipeline。

回调返回的首个错误会终止当前渲染调用，未完成的 Recorder 不会被提交。

Overlay 阶段具有以下固定语义：

- `color_input` 与 `color_output` 是同一个显示空间目标，回调必须使用 `LOAD` 保留场景颜色。
- 不提供深度、阴影、IBL、Renderable 或 Draw Binding；`payload_count` 固定为零。
- `exposure_scale` 固定为 1，UI 不再参与场景曝光和 Tone Mapping。
- `encode_srgb` 为 1 时，UNORM 输出需要由 Shader 编码 sRGB；为 0 时由 sRGB Attachment 完成编码。
- 每个 View 分别调用一次；离屏提交和 Swapchain Frame 共用相同的 Render Graph 构建路径。

## 场景背景

`granit_render_pipeline_render_desc::clear_color` 设置 HDR 场景目标的背景清屏色，随后与场景一起经过
曝光和 Tone Mapping。Version 1 描述以及未显式赋值的 Version 2 描述保持不透明黑色，因此已有
调用不改变行为。颜色分量必须为有限值；多 View 简写和输出数组共用本次渲染描述中的背景色。

该字段只负责稳定纯色背景，不代表天空盒或 HDRI 本身。

## 环境光

Version 3 渲染描述可以通过 `environment` 借用一组预处理环境资源：漫反射 Irradiance Cube、
带 Mip 的 GGX 预过滤 Cube 和二维 BRDF LUT。三个 Texture View 必须同时非零、属于当前 Renderer，
并保持有效直到下一次渲染改用不同环境或 Pipeline 被销毁；Pipeline 不接管其所有权。
`rotation_radians` 必须为有限值，
`intensity` 和 `prefiltered_max_mip` 必须为有限非负值。

该输入由无录制回调的自动 PBR 路径消费。自定义录制回调继续通过 Record Info 接收 Pipeline 的默认
IBL 资源，避免外部纹理句柄与默认 `ibl_group` 不一致；自定义路径应自行管理环境 Bind Group。

环境为空时保留旧行为：内部占位纹理绑定有效，但 IBL 强度为零。公共接口只接收 GPU 资源，不规定
HDRI 文件格式、资产系统或离线卷积工具；这些仍由应用决定。

## GPU 阶段指标

调用 `granit_render_pipeline_metrics_enable` 可启用可选 GPU Timestamp Query。后端不支持时返回
`GRANIT_ERROR_UNSUPPORTED`。之后使用 `granit_render_pipeline_get_metrics` 读取最近一次已完成样本；
尚未产生或完成样本时返回 `GRANIT_ERROR_NOT_READY`，不会伪造零耗时。

`granit_render_pipeline_metrics` 返回递增样本序号，以及 Shadow、Opaque、Tone Mapping 阶段和从
首个已测阶段开始到 Tone Mapping 结束的 GPU 纳秒数。它不包含后续 Debug Draw、Canvas、Overlay、
CPU 帧时间、帧槽等待或 Present 等待，因此不能与 CPU 墙钟相加。Timestamp Query Pool 按真实
Frame Slot 隔离，只在槽位完成并再次复用后读取；离屏同步路径可在本次执行完成后读取。指标回读
暂不可用不会把已经提交成功的渲染改判为失败。

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
- 不包含透明 PBR、Bindless、Clustered Forward 或 Deferred。
- Overlay 路径依次支持世界 Debug Draw、Canvas Draw List，并保留用户回调作为最终自定义扩展点。
- 默认 IBL 占位资源由 Pipeline 内部持有；调用者可按次覆盖预处理环境资源。
- 自动 PBR 路径支持 1× 或 4× MSAA；MSAA 只处理几何边缘，不替代纹理 Mipmap、各向异性过滤
  或 Shader 内的高光抗锯齿。
- 同一 Pipeline 不支持并发渲染，多 View 仍按独立输出顺序执行。

更底层的自定义渲染流程可直接使用 [Command Recorder](command-recorder.md) 和
[Graphics Pipeline](pipeline.md)，无需经过本参考管线。
