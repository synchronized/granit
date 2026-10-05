<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-82 教程能力矩阵验收

## 结论

S-82A～S-82D 已完成。Granit 新增三个跨后端能力探针，未新增公共 API；OIT、SVT、Stencil 和
GPU-driven 多批次仍保持明确的未支持/后续评估边界。

## 交付内容

- `09_particles`：Storage Buffer、动态粒子数量、逐帧更新和 Alpha 混合；
- `10_transparency`：Opaque、Mask、Blend、对象级提交顺序和深度测试边界；
- `11_capabilities`：Renderer limits、Texture Format Usage 和 Filterable 能力查询；
- 能力矩阵、三篇教程文档、Vulkan/WebGPU Shader Library 和浏览器页面入口。

## 验证结果

- Windows Clang 完整构建通过；CTest 108/108 通过；
- 新增三个教程的 Windows Smoke 全部通过；
- Linux/Windows Consumer、Native、Integration、Documentation 检查通过；
- Emscripten 浏览器矩阵通过，首次运行中的 Dawn/WebGPU 与像素测试波动通过重跑恢复；
- 远端 PR #129 的浏览器 Tutorial、Platform Smoke、Model Viewer 和 ImGui 作业全部通过。

## 能力边界

本阶段没有把教程需求直接提升为公共 API。OIT、透明阴影、折射、SVT/虚拟纹理、Stencil State
和 GPU-driven 多批次需要独立的跨后端语义与验收证据，留待后续版本重新评估。

## 发布入口

- [PR #129](https://github.com/synchronized/granit/pull/129)
- [S-82 计划](../plans/S-82-0.45.0-tutorial-capability-matrix.md)
