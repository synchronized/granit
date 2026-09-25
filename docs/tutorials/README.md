<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Granit 特性教程

教程通过五个可运行程序介绍 Granit 的主要渲染路径和单项图形特性。每个程序都能独立阅读，不要求按十个近似项目
逐章复制代码。完整应用和集成展示位于 [Samples 指南](../guides/examples.md)，接口细节以
[Reference](../README.md#接口参考) 为准。

| 教程 | 主要内容 | 运行结果 | 源码 |
|---|---|---|---|
| [01：纹理立方体](01-cube.md) | Window、低层 Renderer、Shader、Texture、Mesh、深度、Canvas | 可交互的旋转纹理立方体 | [01_cube](../../examples/tutorials/01_cube) |
| [02：PBR 资产](02-pbr-assets.md) | HLSL-first、Material、Scene、阴影、HDR、后处理 | 带控制面板的 PBR 场景 | [02_pbr_assets](../../examples/tutorials/02_pbr_assets) |
| [03：实例绘制](03-instancing.md) | 每实例 Vertex Buffer、动态 Uniform、单次实例 Draw | 45 个动画彩色立方体 | [03_instancing](../../examples/tutorials/03_instancing) |
| [04：Raymarch](04-raymarch.md) | 全屏三角形、SDF、法线估计、Resize | 程序化球体、方块和地面 | [04_raymarch](../../examples/tutorials/04_raymarch) |
| [05：Metaballs](05-metaballs.md) | 平滑 SDF 并集、动画参数、隐式曲面光照 | 五个融合的动画球体 | [05_metaballs](../../examples/tutorials/05_metaballs) |

五个教程都复用仓库私有的 `examples/common/application`。它只处理 Window、Renderer 异步初始化、
Surface、Swapchain、事件循环、Resize、Acquire 与 Present；Shader、资源、命令录制和场景仍留在
教程中，便于直接观察 Granit 公共 C++ 接口。

桌面使用 Vulkan，浏览器使用 Emscripten WebGPU。两端编译同一教程主体，平台差异由 Window Loop
与 Renderer Backend 处理。构建环境和 Shader Toolchain 配置见[构建指南](../guides/build.md)。
