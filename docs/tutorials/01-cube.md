<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 01：纹理立方体

本教程用低层 Renderer 路径绘制一个旋转的纹理立方体，并在同一 Backbuffer 上叠加 ImGui。完整源码
位于 [`examples/tutorials/01_cube`](../../examples/tutorials/01_cube)。它集中展示组成实时渲染程序所需
的基础对象，公共 Application 只省略各示例都会重复的平台生命周期。

## 程序组成

初始化阶段依次完成：

1. Application 创建 Window、Renderer、Surface 和 Swapchain。
2. 教程加载构建期生成的 Shader Library，并按逻辑名称创建顶点和片元 Shader。
3. 创建纹理、Sampler、顶点/索引 Buffer、Mesh、动态 Uniform Buffer 和 Graphics Pipeline。
4. 创建与 Swapchain 尺寸一致的深度纹理，以及用于 ImGui 的 Canvas Draw List 和字体纹理。

每帧的调用顺序是：

```text
process_events → acquire → frame_context.begin
  → 更新相机 Uniform
  → begin_rendering → Mesh draw → end_rendering
  → Canvas record → submit → present
```

Resize 或 `out_of_date` 会触发 Swapchain 重建。教程随后重建深度目标，并只在颜色格式改变时重建
Graphics Pipeline。窗口最小化产生零尺寸 framebuffer 时暂停 Acquire。

## Shader 与资源绑定

[`cube.hlsl`](../../examples/tutorials/01_cube/cube.hlsl) 是 HLSL-first 作者输入。CMake 使用
[`cube.grshlib.json`](../../examples/tutorials/01_cube/cube.grshlib.json) 构建同时包含 Vulkan SPIR-V
与 WebGPU WGSL 的 Shader Library，运行时通过 `mesh.vertex` 和 `mesh.fragment` 查找入口。

Bind Group 0 包含动态相机 Uniform、采样纹理和 Sampler。每个 Frame Slot 使用不同的 Uniform
偏移，避免 CPU 覆盖仍由 GPU 读取的数据。Mesh 保存 Buffer、顶点布局和索引 Draw 参数，Pipeline
保存 Shader、目标格式、深度状态和资源布局。

## ImGui 与 Canvas

Window/Input 事件由 Application 转发给 ImGui。教程把 `ImDrawData` 转换为 Granit
`canvas_draw_list`，再在 3D Render Pass 后以 `load` 操作记录 Canvas Pass。ImGui 控制面板可暂停旋转，
并显示自定义 Texture ID，验证字体、纹理解析、裁剪和输入路径。

## 构建与验证

Windows Clang：

```powershell
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug --target granit_tutorial_01_cube
build/windows-clang-debug/bin/granit_tutorial_01_cube.exe
```

Emscripten WebGPU：

```powershell
cmake --preset emscripten-debug
cmake --build --preset emscripten-debug --target granit_tutorial_01_cube
python -m http.server 8000 --directory build/emscripten-debug/web
```

浏览器打开 `http://localhost:8000/granit_tutorial_01_cube.html`。自动验证会检查多帧推进、Canvas
非空以及 Resize 后继续呈现：

```powershell
ctest --preset windows-clang-debug -R "^granit\.tutorial\.01_cube$" --output-on-failure
npm --prefix tests/web run test:tutorial-01 -- ../../build/emscripten-debug/web
```

Frame 所有权、失败 Cancel 和提交层次见
[Frame 与命令录制分层](../concepts/frame-and-command-lifecycle.md)。
