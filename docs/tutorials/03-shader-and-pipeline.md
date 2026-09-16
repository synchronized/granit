<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 03：创建 Shader 与 Graphics Pipeline

本教程在无窗口的离屏目标中创建一个三角形，串起 Shader Library、Pipeline Layout、Graphics
Pipeline、Command Recorder 和 Texture Readback。完整程序位于
[`examples/samples/triangle`](../../examples/samples/triangle)，本页只保留理解流程所需的关键
代码。

## 前置条件

- 已完成[创建第一个 Renderer](01-first-renderer.md)；
- 构建机可以使用锁定的 Shader Toolchain；
- 已安装或构建 AssetTools，因为示例在构建期生成 Shader Library。

## 1. 描述跨后端 Shader

示例使用 HLSL 源文件和 `triangle.grshlib.json` 清单声明顶点与片段入口：

```json
{
  "format_version": 1,
  "name": "triangle",
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

构建系统调用 AssetTools 生成 `.grshlib` 和索引，再生成稳定的 Shader Content ID。应用运行时只
加载归档和 Content ID，不依赖 DXC、Tint 或 Vulkan SDK。

## 2. 创建 Shader 和 Pipeline

创建 Renderer 后加载归档，并按 Content ID 选择当前后端的 Shader 变体：

```cpp
granit::shader_library library;
check(library.initialize(renderer.native_handle(), archive));

granit::shader vertex_shader;
check(library.create_shader(triangle_vertex_id, vertex_shader));

granit::shader fragment_shader;
check(library.create_shader(triangle_fragment_id, fragment_shader));
```

本示例的 Shader 没有资源绑定，因此 Pipeline Layout 为空：

```cpp
granit::pipeline_layout layout;
check(layout.initialize(renderer.native_handle()));

constexpr auto format = granit::texture_format::rgba8_unorm;
granit::graphics_pipeline pipeline;
check(pipeline.initialize(renderer.native_handle(), {
    .layout = layout.native_handle(),
    .vertex_shader = vertex_shader.native_handle(),
    .fragment_shader = fragment_shader.native_handle(),
    .color_formats = std::span{&format, 1},
}));
```

顶点 Shader 使用 `SV_VertexID` 生成三个顶点，因此本阶段不需要 Vertex Buffer。资源绑定、顶点
布局和 Upload Batch 会在后续资源教程中加入。

## 3. 创建离屏目标

输出 Texture 必须同时支持颜色附件和传输源，才能完成绘制后读取：

```cpp
granit::texture output;
check(output.initialize(renderer.native_handle(), {
    .format = format,
    .usage = granit::texture_usage::color_attachment |
             granit::texture_usage::transfer_source,
    .width = 64,
    .height = 64,
}));

granit::texture_view output_view;
check(output_view.initialize(renderer.native_handle(), output.native_handle()));
```

纹理格式和 Usage 的准确限制见[Texture 参考](../reference/texture.md)。应用不能假定任意格式都
支持颜色附件或回读，应在需要适配不同设备时查询格式能力。

## 4. 录制并提交绘制

Command Recorder 的基本顺序是：开始录制、设置动态状态、开始 Rendering、绑定 Pipeline、绘制、
结束 Rendering、结束录制并提交：

```cpp
granit::command_recorder recorder;
check(recorder.initialize(renderer.native_handle()));
check(recorder.begin());
check(recorder.bind_graphics_pipeline(pipeline.native_handle()));
check(recorder.begin_rendering(rendering));
check(recorder.draw(3));
check(recorder.end_rendering());
check(recorder.end());
check(recorder.submit());
```

提交后通过 `renderer.process_events()` 推进完成状态，再读取 64×64 目标的中心像素。示例 Smoke
会输出中心像素，用于确认渲染和回读路径均已执行。

## 5. 构建和验证

```powershell
cmake --preset windows-vs2022-debug
cmake --build --preset windows-vs2022-debug --target granit_triangle_example
ctest --test-dir build/windows-vs2022-debug -C Debug `
  -R "^granit\.example\.triangle$" --output-on-failure
```

Shader Toolchain 只参与构建期资产生成，不进入应用运行时依赖。Shader Library、缓存和变体选择
的完整契约见[Shader Library 参考](../reference/shader-library.md)和
[AssetTools SDK 参考](../reference/asset-tools.md)。

## 常见问题

- **Shader Library 生成失败**：确认 `GRANIT_SHADER_TOOLCHAIN_ROOT` 指向包含 DXC 和 Tint 的
  锁定工具链根目录。
- **Pipeline 创建失败**：检查 Shader 的阶段、入口、目标格式和 Pipeline Layout 是否匹配。
- **回读失败**：确认输出 Texture 包含 `transfer_source` Usage，并在提交后推进 Renderer 事件。
