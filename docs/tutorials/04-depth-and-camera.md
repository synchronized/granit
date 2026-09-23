<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 04：加入深度与相机

本章把屏幕空间图形改为三维立方体，增加 Vertex Buffer、Index Buffer、Depth Texture、Uniform
Buffer 和相机矩阵。完成后应看到一个能够正确遮挡、缓慢旋转的立方体。

## 1. 上传顶点和索引

把位置和 UV 写入 Device Local Vertex Buffer，把索引写入 Index Buffer，并在 Graphics Pipeline 中
声明与 C++ 顶点结构完全一致的 stride、attribute location 和 format。Upload Batch 可以在一次提交中
写入两个 Buffer。

本章的 `vertex` 包含三个位置分量和两个 UV 分量。Pipeline 中的两个 Attribute 必须分别使用
`float32x3`、`float32x2`，第二个 Attribute 的 Offset 为三个 `float`：

```cpp
const std::array attributes{
    granit::vertex_attribute{.location = 0, .format = granit::vertex_format::float32x3},
    granit::vertex_attribute{.location = 1,
                             .format = granit::vertex_format::float32x2,
                             .offset = sizeof(float) * 3},
};
```

## 2. 增加深度附件

创建与 framebuffer 同尺寸的 Depth Texture 和 View，并把深度格式加入 Pipeline。每帧 Rendering
同时提供颜色附件和深度附件；深度值清除为远平面。Resize 时，Depth Texture 与 Swapchain 一起重建。

```cpp
const granit::depth_stencil_attachment_desc depth{
    .view = depth_view.ref(), .clear_value = {.depth = 1.0F}};
const granit::rendering_desc rendering{
    .color_attachments = std::span{&color, 1},
    .depth_stencil_attachment = &depth,
    .area = {0, 0, width, height},
};
```

## 3. 更新相机 Uniform

每帧计算 Model、View、Projection 矩阵，把组合结果写入当前帧槽对应的 Uniform 区域。不要让多个
在途帧覆盖同一块仍被 GPU 使用的动态数据；优先使用 Frame Context 暴露的帧槽信息管理偏移。

相机使用 framebuffer 宽高比。最小化产生零尺寸时跳过矩阵除法和帧获取。

先读取 `renderer_limits::uniform_buffer_offset_alignment`，把 64 字节矩阵向上对齐并为每个在途帧
分配一个区域。`frame_recording::frame_slot()` 选择本帧区域；Bind Group 保持不变，只在录制时传入
动态 Offset。这样上一帧的 GPU 读取不会与本帧 CPU 写入同一区域冲突。

## 4. 绘制立方体

录制顺序变为：绑定 Pipeline、Vertex Buffer、Index Buffer、相机 Bind Group，然后执行 Indexed Draw。
背面剔除、正面绕序和深度比较必须与 Shader 输出的坐标约定一致。

## 5. 构建并运行

完整源码和 Shader 位于
[`examples/tutorials/04_depth_and_camera`](../../examples/tutorials/04_depth_and_camera)：

```powershell
cmake --preset windows-clang-debug -DGRANIT_SHADER_TOOLCHAIN_MODE=auto
cmake --build --preset windows-clang-debug --target granit_tutorial_04_depth_and_camera
.\build\windows-clang-debug\bin\granit_tutorial_04_depth_and_camera.exe
```

自动 Smoke 绘制三帧并验证一次 Resize/Recreate 路径：

```powershell
ctest --preset windows-clang-debug -R granit.tutorial.04_depth_and_camera --output-on-failure
```

## 6. 验收

- 立方体远侧表面被正确遮挡。
- 改变窗口宽高比后物体不被拉伸。
- 连续旋转时没有帧间 Uniform 闪烁。
- Resize 后深度附件尺寸与 Backbuffer 一致。

完整程序见 [`main.cpp`](../../examples/tutorials/04_depth_and_camera/main.cpp)。销毁时先释放 Pipeline、
Bind Group 和 Layout，再释放 Buffer、Texture View 与 Texture，最后释放帧和呈现资源。

Buffer、附件和帧槽规则见 [Buffer](../reference/buffer.md)、
[Render Target Attachment](../reference/render-target.md)和 [Frame Context](../reference/frame-context.md)。

[上一章：添加纹理](03-texture.md) · [下一章：组织 Mesh](05-mesh.md)
