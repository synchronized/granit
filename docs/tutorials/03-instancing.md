<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 03：实例绘制

本教程用一次索引绘制提交 45 个立方体，展示每顶点数据、每实例数据和动态 Uniform 如何共同组成
Graphics Pipeline。完整源码位于
[`examples/tutorials/03_instancing`](../../examples/tutorials/03_instancing)。

## 数据组织

顶点 Buffer 保存立方体位置，索引 Buffer 保存三角形拓扑。另一个 Vertex Buffer 按实例保存位移、
缩放和颜色，并将步进模式设为 `per_instance`。Shader 通过不同输入位置读取两类数据，因此不需要
为每个立方体分别创建 Mesh、Bind Group 或 Draw。

相机的投影视图矩阵与时间写入动态 Uniform Buffer。每个在途 Frame Slot 使用对齐后的独立区域，
避免 CPU 更新覆盖 GPU 尚未读取的数据。`draw_indexed` 的实例数设为 45，这是本教程所验证的核心
能力。

## Shader 与逐帧流程

[`instancing.hlsl`](../../examples/tutorials/03_instancing/instancing.hlsl) 经 Shader Toolchain 同时生成
SPIR-V 与 WGSL，并以逻辑名称 `instancing.vertex`、`instancing.fragment` 从内嵌 Shader Library
加载。桌面 Vulkan 和浏览器 WebGPU 使用同一份 C++ 与 HLSL 输入。

每帧依次更新 Uniform、开始 Rendering、绑定 Pipeline 与 Bind Group，再执行一次实例绘制。Smoke
模式固定时间和实例布局，使像素结果可重复。

## 构建与验证

```powershell
cmake --preset windows-clang-debug
cmake --build build/windows-clang-debug --target granit_tutorial_03_instancing
ctest --test-dir build/windows-clang-debug -R "^granit\.tutorial\.03_instancing$" `
  --output-on-failure
```

浏览器构建产生 `granit_tutorial_03_instancing.html`。自动测试检查页面初始化、帧推进、实例数量、
中心与背景像素差异，以及 Resize 后的 Swapchain 重建。

