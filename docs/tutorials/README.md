<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Granit 特性教程

教程通过八个可运行程序介绍 Granit 的主要渲染路径、单项图形特性和第三方宿主集成。v0.45.0 将
继续增加三个能力探针；每个程序都能独立阅读，不要求按多个近似项目逐章复制代码。完整应用和
集成展示位于 [Samples 指南](../guides/examples.md)，接口细节以 [Reference](../README.md#接口参考)
为准。当前覆盖与缺口分类见[能力矩阵](capability-matrix.md)。

| 教程 | 主要内容 | 运行结果 | 源码 |
|---|---|---|---|
| [01：纹理立方体](01-cube.md) | Window、低层 Renderer、Shader、Texture、Mesh、深度、Canvas | 可交互的旋转纹理立方体 | [01_cube](../../examples/tutorials/01_cube) |
| [02：PBR 资产](02-pbr-assets.md) | HLSL-first、Material、Scene、阴影、HDR、后处理 | 带控制面板的 PBR 场景 | [02_pbr_assets](../../examples/tutorials/02_pbr_assets) |
| [03：实例绘制](03-instancing.md) | 每实例 Vertex Buffer、动态 Uniform、单次实例 Draw | 45 个动画彩色立方体 | [03_instancing](../../examples/tutorials/03_instancing) |
| [04：Raymarch](04-raymarch.md) | 全屏三角形、SDF、法线估计、Resize | 程序化球体、方块和地面 | [04_raymarch](../../examples/tutorials/04_raymarch) |
| [05：Metaballs](05-metaballs.md) | 平滑 SDF 并集、动画参数、隐式曲面光照 | 五个融合的动画球体 | [05_metaballs](../../examples/tutorials/05_metaballs) |
| [06：GPU Marching Cubes](06-marching-cubes.md) | Compute、Transient Buffer、Atomic、Indirect Draw | GPU 生成的动态 Metaballs 网格 | [06_marching_cubes](../../examples/tutorials/06_marching_cubes) |
| [07：Deferred Rendering](07-deferred.md) | MRT、G-buffer、中间纹理采样、多 Pass、Resize | 多点光源照亮的实例场景与 G-buffer 调试视图 | [07_deferred](../../examples/tutorials/07_deferred) |
| [08：SDL3 + ImGui](08-sdl-imgui.md) | 外部 SDL Window、ImGui Platform Backend、Surface、Canvas | SDL3 宿主中的跨后端 ImGui 面板 | [08_sdl_imgui](../../examples/tutorials/08_sdl_imgui) |
| 09：Particles | 动态 Storage Buffer、粒子几何、透明混合 | S-82B 已实现，待跨平台验收 | [09_particles](../../examples/tutorials/09_particles) |
| 10：Transparency | Opaque、Mask、Blend、对象级排序 | S-82C 已实现，待跨平台验收 | [10_transparency](../../examples/tutorials/10_transparency) |
| 11：Capabilities | Format、Usage、Sampler、Mip 和 Renderer limits | S-82D 已实现，待跨平台验收 | [11_capabilities](../../examples/tutorials/11_capabilities) |
| [12：Bindless 能力探针](12-bindless-probe.md) | Descriptor Indexing 前置能力、Resource Table 边界和传统路径回退 | 显示设备能力，不启用默认 Bindless | [12_bindless_probe](../../examples/tutorials/12_bindless_probe) |

前七个渲染教程复用仓库私有的 `examples/common/application`。它只处理 Window、Renderer 异步初始化、
Surface、Swapchain、事件循环、Resize、Acquire 与 Present；Shader、资源、命令录制和场景仍留在
教程中，便于直接观察 Granit 公共 C++ 接口。`08_sdl_imgui` 刻意不使用该 Application：它演示
调用方拥有 SDL Window 和事件循环时，如何只把 Surface 与 ImGui Draw Data 交给 Granit。

桌面使用 Vulkan，浏览器使用 Emscripten WebGPU。两端编译同一教程主体，平台差异由 Window Loop
与 Renderer Backend 处理。构建环境和 Shader Toolchain 配置见[构建指南](../guides/build.md)。
