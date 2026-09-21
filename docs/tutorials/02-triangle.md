<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 02：绘制三角形

本章保留第一章的窗口和帧循环，增加 HLSL Shader Library、Pipeline Layout 和 Graphics Pipeline。
完成后，清屏颜色上会出现一个三角形。

## 1. 描述 Shader Library

使用一个 HLSL 文件声明 `vertex_main` 和 `fragment_main`。顶点入口先通过 `SV_VertexID` 生成三个
位置，因此本章不引入 Vertex Buffer。清单把两个入口映射为稳定 Shader 名称：

```json
{
  "format_version": 1,
  "name": "tutorial_triangle",
  "target_profile": "portable",
  "target_backends": ["vulkan", "webgpu"],
  "shaders": [
    {"name": "triangle.vertex", "source": "triangle.hlsl", "stage": "vertex",
     "entry_point": "vertex_main"},
    {"name": "triangle.fragment", "source": "triangle.hlsl", "stage": "fragment",
     "entry_point": "fragment_main"}
  ]
}
```

AssetTools 在构建期生成 `.grshlib`、索引和 Content ID。运行时只加载归档，不依赖 Shader 编译器。

## 2. 创建 Shader 与 Pipeline

```cpp
granit::shader_library library;
check(library.initialize(renderer.native_handle(), archive));

granit::shader vertex_shader;
granit::shader fragment_shader;
check(library.create_shader(triangle_vertex_id, vertex_shader));
check(library.create_shader(triangle_fragment_id, fragment_shader));

granit::pipeline_layout layout;
check(layout.initialize(renderer.native_handle()));
```

Graphics Pipeline 的颜色格式必须与 Swapchain 格式一致。窗口重建后若格式变化，也要重建依赖该格式
的 Pipeline。

## 3. 在帧循环中 Draw

在第一章的 `begin_rendering()` 与 `end_rendering()` 之间增加：

```cpp
check(recorder.set_viewports(0, std::span{&viewport, 1}));
check(recorder.set_scissors(0, std::span{&scissor, 1}));
check(recorder.bind_graphics_pipeline(pipeline.native_handle()));
check(recorder.draw(3));
```

Viewport 和 Scissor 使用当前 framebuffer 尺寸。不要把逻辑窗口尺寸直接当成高 DPI framebuffer 尺寸。

## 4. 验收

- 三角形稳定显示在窗口中央。
- Resize 后比例和裁剪区域正确。
- Shader Toolchain 只在构建阶段运行。
- 运行目录缺少 `.grshlib` 时给出明确的文件读取错误。

进一步约束见 [Shader Library](../reference/shader-library.md)、[Graphics Pipeline](../reference/pipeline.md)
和 [Command Recorder](../reference/command-recorder.md)。下一章为图形增加纹理资源和绑定。

[上一章：创建窗口并清屏](01-window.md) · [下一章：添加纹理](03-texture.md)
