<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 09：Particle Generator

本教程使用一个 Storage Buffer 保存粒子状态，顶点 Shader 通过 `SV_InstanceID` 读取实例数据，
共享低面数 Mesh 绘制球体、正方体或长方体，并通过 Alpha 混合绘制。它是 S-98B 的交互式能力
示例，完整源码位于
[`examples/tutorials/09_particles`](../../examples/tutorials/09_particles)。

## 验证目标

- 动态更新 GPU Storage Buffer，不创建 Particle 专用公共 API；
- 使用实例化绘制和共享 Mesh 生成可变数量的三维粒子；
- 通过 ImGui 调整粒子数量、形状、颜色、方向、加速度、生命周期、尺寸和淡出；
- 验证 Alpha 混合与跨后端资源绑定；
- Smoke 模式固定数量和时间，便于桌面与浏览器回归。

粒子积分和颜色布局属于教程本地逻辑。GPU simulation、粒子排序、OIT、透明阴影和虚拟纹理不
属于本教程。

## 构建与验证

```powershell
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug --target granit_tutorial_09_particles
ctest --test-dir build/windows-clang-debug -R "^granit\.tutorial\.09_particles$" --output-on-failure
```

浏览器构建使用 `emscripten-debug` preset，页面入口为
`granit_tutorial_09_particles.html`。验收应检查多帧推进、非空 Canvas、Resize 和 WebGPU
Validation 消息。
