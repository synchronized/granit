<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 05：Metaballs

本教程在全屏 Fragment Raymarch 中组合五个动画球体，通过平滑并集形成连续的隐式曲面。完整源码
位于 [`examples/tutorials/05_metaballs`](../../examples/tutorials/05_metaballs)。

## 隐式曲面

Shader 根据时间计算五个球心，用多项式平滑并集合并各球体的有符号距离，再通过中心差分估算法线。
颜色同时反映高度、漫反射与 Fresnel 边缘光。动画只改变 Uniform 中的时间，CPU 不需要每帧生成
网格或上传球体顶点。

本实现刻意保留 Fragment 路径：它能用现有 Graphics Pipeline 清楚展示效果，也能验证动态参数、
循环和跨后端 Shader 生成。Compute 网格提取、Marching Cubes 与 3D Texture 不属于本入门教程。

## 构建与验证

```powershell
cmake --preset windows-clang-debug
cmake --build build/windows-clang-debug --target granit_tutorial_05_metaballs
ctest --test-dir build/windows-clang-debug -R "^granit\.tutorial\.05_metaballs$" `
  --output-on-failure
```

Smoke 模式固定动画时间。浏览器自动测试检查五个球体这一特性值、帧推进、物体与背景差异、Resize
恢复和 Console 错误。
