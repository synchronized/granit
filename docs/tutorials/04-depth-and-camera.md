<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 04：加入深度与相机

本章把屏幕空间图形改为三维立方体，增加 Vertex Buffer、Index Buffer、Depth Texture、Uniform
Buffer 和相机矩阵。完成后应看到一个能够正确遮挡、缓慢旋转的立方体。

## 1. 上传顶点和索引

把位置和 UV 写入 Device Local Vertex Buffer，把索引写入 Index Buffer，并在 Graphics Pipeline 中
声明与 C++ 顶点结构完全一致的 stride、attribute location 和 format。Upload Batch 可以在一次提交中
写入两个 Buffer。

## 2. 增加深度附件

创建与 framebuffer 同尺寸的 Depth Texture 和 View，并把深度格式加入 Pipeline。每帧 Rendering
同时提供颜色附件和深度附件；深度值清除为远平面。Resize 时，Depth Texture 与 Swapchain 一起重建。

## 3. 更新相机 Uniform

每帧计算 Model、View、Projection 矩阵，把组合结果写入当前帧槽对应的 Uniform 区域。不要让多个
在途帧覆盖同一块仍被 GPU 使用的动态数据；优先使用 Frame Context 暴露的帧槽信息管理偏移。

相机使用 framebuffer 宽高比。最小化产生零尺寸时跳过矩阵除法和帧获取。

## 4. 绘制立方体

录制顺序变为：绑定 Pipeline、Vertex Buffer、Index Buffer、相机 Bind Group，然后执行 Indexed Draw。
背面剔除、正面绕序和深度比较必须与 Shader 输出的坐标约定一致。

## 5. 验收

- 立方体远侧表面被正确遮挡。
- 改变窗口宽高比后物体不被拉伸。
- 连续旋转时没有帧间 Uniform 闪烁。
- Resize 后深度附件尺寸与 Backbuffer 一致。

Buffer、附件和帧槽规则见 [Buffer](../reference/buffer.md)、
[Render Target Attachment](../reference/render-target.md)和 [Frame Context](../reference/frame-context.md)。

[上一章：添加纹理](03-texture.md) · [下一章：组织 Mesh](05-mesh.md)
