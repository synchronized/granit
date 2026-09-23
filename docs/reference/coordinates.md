<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 坐标系统约定

Granit 向所有 Renderer backend 暴露同一套坐标语义。应用、引擎和 Shader 不应查询 backend 后再
翻转相机、顶点位置、Viewport 或 UV；Vulkan 与 WebGPU 的原生差异由 Granit 后端处理。

## 三维世界与裁剪空间

RenderPipeline 使用右手三维坐标：`+X` 向右、`+Y` 向上，View Space 中相机朝 `-Z`。公共数学库的
`look_at_rh`、`perspective_rh_zo` 和 `orthographic_rh_zo` 直接生成符合该约定的矩阵。

Vertex Shader 输出的逻辑裁剪空间为：

- X、Y 的可见范围是 `-1..1`，`+Y` 映射到屏幕上方；
- 深度范围是 `0..1`；
- `front_face` 的含义不随 backend 改变；
- View Projection、Debug Draw 与自定义 Shader 共用该约定。

Vulkan 后端使用负高度原生 Viewport；Vulkan 与 WebGPU 分别按原生窗口映射转换 Front Face。
这些转换不进入公共 API。

## Viewport、Scissor 与二维坐标

Viewport、Scissor、纹理区域、回读像素和 Canvas 使用左上原点：X 向右、Y 向下。公开 Viewport 的
`width`、`height` 均为正值，调用方不传负高度来适配 Vulkan。

Canvas、Text 和屏幕空间 Debug Draw 使用左上原点的像素单位。它们在内部映射到统一逻辑裁剪空间，
因此桌面 Vulkan 与浏览器 WebGPU 的布局方向一致。

## 纹理 UV

普通二维纹理使用左上原点的 `0..1` UV：`(0, 0)` 是左上角，U 向右，V 向下。Render Target 作为
纹理采样时沿用同一规则，色调映射和后处理不额外暴露 backend 翻转。

图片格式或外部 API 使用其他原点时，资源导入层应在上传或转换阶段明确处理。相机矩阵不能用来
补偿纹理方向。
