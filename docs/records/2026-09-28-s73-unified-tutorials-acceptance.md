<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-28 S-73 统一交互式 Tutorial 验收

S-73 已统一现有五个 Tutorial 的 ImGui、性能面板和轨道相机，并完成动态实例范围与章节参数迁移。
本记录保存阶段结果；当前设计以
[S-73 计划](../plans/S-73-0.39.0-unified-interactive-tutorials.md)和源码为准。

## 实施结果

- 公共 Tutorial Runtime 统一 ImGui Context、Granit 主题、字体、输入、Canvas 和帧指标；
- 公共 Orbit Camera 由 Model Viewer 与五个 Tutorial 复用，支持聚焦、旋转、缩放、重置和自动环绕；
- Cube 与 PBR Assets 保留纹理和材质参数，并使用统一面板与相机交互；
- Instancing 支持 1～32 行列、动态 Draw 实例范围，以及实例化与逐对象 Draw 对照；
- Raymarch 与 Metaballs 的相机和效果参数进入 Shader，不再依赖固定观察点；
- 所有 Tutorial 禁用 `imgui.ini`，Smoke 使用确定参数并生成非空 Canvas。

## 验证结果

| 环境 | 验证 | 结果 |
|---|---|---|
| Windows Clang Debug | Common Camera、ImGui、五个 Tutorial、Model Viewer 与 Pipeline API | 14/14 通过 |
| Emscripten Debug | 完整构建与 `ctest --preset emscripten-debug` | 16/16 通过 |
| Chrome WebGPU | Tutorial 01～05 多帧、Canvas、特性值、Resize 与像素检查 | 通过 |

阶段验收没有修改版本号或创建 Release。后续 S-74 直接复用本阶段的 Tutorial Runtime 与 Orbit
Camera，实现 GPU Marching Cubes。

## 公共接口影响

实验性 Mesh Draw 新增逐次 `instance_count` 与 `first_instance` 描述；创建时保存的实例范围仍作为
无参数绘制的默认值。C/C++ 行为测试覆盖零数量、容量越界及错误资源关系。
