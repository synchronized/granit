<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 04：Raymarch

本教程绘制一个由 `SV_VertexID` 生成的全屏三角形，并在 Fragment Shader 中完成相机射线、距离场
步进、法线估计和光照。完整源码位于
[`examples/tutorials/04_raymarch`](../../examples/tutorials/04_raymarch)。

## 全屏绘制

顶点 Shader 直接生成三个裁剪空间顶点，所以 Pipeline 不需要 Vertex Buffer 或 Vertex Layout。
Fragment Shader 根据 `SV_Position` 和 framebuffer 尺寸恢复纵横比正确的屏幕坐标，再对球体、方块
与地面组成的 SDF 场景最多执行 80 次步进。

窗口 Resize 后 Application 重建 Swapchain，教程把新的 framebuffer 尺寸写入 Uniform。Shader
显式遵循 Granit 的屏幕与纹理坐标约定，场景背景上下不对称，便于自动发现 Y 轴翻转错误。

## Shader 与资源

[`raymarch.hlsl`](../../examples/tutorials/04_raymarch/raymarch.hlsl) 是唯一作者输入。构建生成同时包含
Vulkan SPIR-V 和 WebGPU WGSL 的 Shader Library；运行时只按逻辑名称加载两个入口。资源绑定仅含
每帧更新的尺寸与时间 Uniform，Smoke 模式固定时间。

## 构建与验证

```powershell
cmake --preset windows-clang-debug
cmake --build build/windows-clang-debug --target granit_tutorial_04_raymarch
ctest --test-dir build/windows-clang-debug -R "^granit\.tutorial\.04_raymarch$" `
  --output-on-failure
```

浏览器自动测试还会比较物体区域与背景像素，并确认 Resize 后继续呈现。
