<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 03：添加纹理

本章在三角形程序中加入 Texture、Texture View、Sampler、Upload Batch 和 Bind Group。为了更清楚地
观察采样结果，可以把三角形改为由两个三角形组成的四边形。完成后应看到一张 2×2 棋盘纹理。

## 1. 创建并上传 Texture

Texture Usage 必须同时覆盖上传目标和 Shader 采样：

```cpp
granit::texture texture;
check(texture.initialize(renderer, {
    .format = granit::texture_format::rgba8_unorm,
    .usage = granit::texture_usage::sampled |
             granit::texture_usage::transfer_destination,
    .width = 2,
    .height = 2,
}));

granit::texture_view texture_view;
check(texture_view.initialize(renderer, texture));
```

使用 Upload Batch 写入四个 RGBA 像素。Batch 会复制调用方字节，成功提交后局部像素数组可以释放。

## 2. 创建 Sampler 与绑定

创建最近邻 Sampler，让 2×2 颜色块保持清晰。随后按照 Shader 清单中的布局创建 Bind Group，把
Texture View 和 Sampler 放入同一资源组。Pipeline Layout 必须引用相同布局；布局、Shader 声明和
实际资源类型不一致时，Pipeline 或绑定创建应失败。

## 3. 在 Fragment Shader 中采样

Vertex Shader 输出 UV，Fragment Shader 使用绑定的 Texture 和 Sampler。为避免不同图形 API 的
坐标约定泄漏到应用，使用 Shader 工具链规定的 portable profile，不在 C++ 中按后端翻转坐标。

录制时，在 `draw()` 前绑定新的 Pipeline 和 Bind Group。Texture、View、Sampler 与 Bind Group 都要
保持到 GPU 不再使用当前帧。

## 4. 验收

- 四个颜色块方向正确且边界清晰。
- 删除 `sampled` Usage 或提供错误布局时能得到稳定错误结果。
- Resize 只重建窗口相关资源，不重复上传不变的纹理。
- 多帧运行后没有 Upload Batch 或 Bind Group 生命周期诊断。

资源规则见 [Texture](../reference/texture.md)、[Sampler](../reference/sampler.md)和
[Upload Batch](../reference/upload-batch.md)。下一章把程序扩展为有深度和相机的三维场景。

[上一章：绘制三角形](02-triangle.md) · [下一章：深度与相机](04-depth-and-camera.md)
