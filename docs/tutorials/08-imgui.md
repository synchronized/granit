<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 08：接入 ImGui 调试界面

本章在 Render Pipeline 输出上叠加 ImGui，把 Window 输入事件转换为 ImGui IO，再把 ImGui Draw Data
转换为 Granit Canvas。完成后，窗口包含可交互的材质参数和性能面板；同一内容也能在浏览器 WebGPU
中运行。

## 1. 转换输入

每帧先由 Window System 处理事件，再把键盘、文本、指针、滚轮、焦点和 DPI 变化交给 ImGui Platform
Backend。输入层只消费 Granit Window 事件，不读取 Win32、XCB、Wayland 或 DOM 原生对象。

## 2. 构建界面

在 `NewFrame` 与 `Render` 之间显示帧时间、材质参数和光照开关。修改材质参数时通过稳定 Parameter ID
更新 Material，不直接访问后端 Uniform Buffer。

## 3. 转换 Draw Data

ImGui Renderer Integration 把 Draw Lists 转换为 Granit Canvas：

- 顶点和索引写入逐帧上传区；
- Clip Rect 转换为 Scissor；
- Font Atlas 和自定义纹理通过 Texture ID 注册表解析；
- Canvas 在 Tone Mapping 后写入同一输出目标。

关闭窗口时先停止生成新 Draw Data，再释放 ImGui Renderer、纹理注册、字体资源和 Window。

## 4. 验证浏览器后端

Emscripten 构建复用同一 ImGui 内容和 Canvas 路径。浏览器只替换 Window 平台壳层与 WebGPU 后端，
材质参数、界面布局和渲染业务代码保持一致。具体构建命令见
[浏览器 WebGPU 指南](../guides/webgpu-browser-example.md)。

## 5. 验收

- 鼠标、键盘、文本输入和滚轮行为正确。
- Resize 与 1×/2× DPI 下字体、裁剪和点击位置一致。
- 自定义 Texture ID 显示正确，失效 ID 不访问已销毁资源。
- 关闭时没有 Draw Data 转换失败或残留 Shader Library 诊断。
- Vulkan 与浏览器 WebGPU 显示相同的固定验证界面。

完整综合应用见[示例程序指南](../guides/examples.md)，浏览器自动验证见
[WebGPU 指南](../guides/webgpu-browser-example.md)。

[上一章：使用 Render Pipeline](07-render-pipeline.md) · [返回教程目录](README.md)
