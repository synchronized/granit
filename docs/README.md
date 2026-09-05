<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Granit 文档中心

这里是 Granit 使用指南、参考资料、架构说明和开发计划的统一入口。根 README 只负责项目介绍与
快速开始；具体事实以本页链接的对应文档为准。

## 推荐阅读顺序

### 第一次使用参考渲染管线

1. [Render Pipeline 离屏渲染教程](tutorials/render-pipeline-offscreen.md)
2. [示例程序及运行方式](guides/examples.md)
3. [Render Pipeline](reference/render-pipeline.md)

### 使用核心 Renderer

1. [构建与安装](guides/build.md)
2. [Renderer](reference/renderer.md)
3. [资源类型](reference/resource-types.md)
4. [Command Recorder](reference/command-recorder.md)
5. [Frame Context](reference/frame-context.md)
6. [线程安全](reference/thread-safety.md)

### 理解高级渲染层

1. [架构与 ABI](concepts/architecture.md)
2. [路线图](roadmap.md)
3. [Render Graph 边界](plans/P-06-render-graph-boundary.md)
4. [高级参考渲染套件](plans/H-07-reference-render-pipeline.md)
5. [Model Viewer 渲染线程计划](plans/S-19-model-viewer-render-thread.md)

### 参与开发

1. [开发规范](guides/development.md)
2. [项目文档规范](../DOCUMENTATION_GUIDE.md)
3. [开发计划索引](plans/README.md)
4. [第三方依赖](../3rd/README.md)
5. [0.9.0 公共 PBR 与渲染资产收敛计划](plans/S-24-0.9.0-public-pbr-assets.md)

## 操作指南

- [构建、测试、安装与 CMake 集成](guides/build.md)
- [发布验收](guides/release.md)
- [从 0.2 迁移到 0.3](guides/migrate-0.2-to-0.3.md)
- [从 0.3 迁移到 0.4](guides/migrate-0.3-to-0.4.md)
- [从 0.4 迁移到 0.5](guides/migrate-0.4-to-0.5.md)
- [从 0.5 迁移到 0.6](guides/migrate-0.5-to-0.6.md)
- [从 0.6 迁移到 0.7](guides/migrate-0.6-to-0.7.md)
- [从 0.7 迁移到 0.8](guides/migrate-0.7-to-0.8.md)
- [从 0.8 迁移到 0.9](guides/migrate-0.8-to-0.9.md)
- [示例程序及运行方式](guides/examples.md)
- [运行跨后端模型查看器](guides/model-viewer.md)
- [运行浏览器 WebGPU 平台 Smoke](guides/webgpu-browser-example.md)
- [同步批量上传](guides/upload-batch.md)
- [纹理同步回读](guides/texture-readback.md)
- [第三方 UI 与字体适配](guides/third-party-ui-adapters.md)

## 教程

- [使用 Render Pipeline 完成第一次离屏渲染](tutorials/render-pipeline-offscreen.md)

## API 与行为参考

### 核心与数学

- [版本与兼容策略](reference/compatibility.md)
- [核心 C API 所有权、错误与扩展契约](reference/c-api-contract.md)
- [Renderer 生命周期与诊断](reference/renderer.md)
- [公共数学值类型](reference/math-types.md)
- [公开对象线程安全矩阵](reference/thread-safety.md)

### GPU 资源

- [资源类型总览](reference/resource-types.md)
- [Buffer](reference/buffer.md)
- [Texture 与 Texture View](reference/texture.md)
- [Sampler](reference/sampler.md)
- [Render Target Attachment](reference/render-target.md)

### 命令与 Pipeline

- [ShaderTools SDK](reference/shader-tools.md)
- [Shader 工具链包清单](reference/shader-toolchain-package.md)
- [Command Recorder](reference/command-recorder.md)
- [Frame Context](reference/frame-context.md)
- [Shader Module](reference/shader.md)
- [Graphics 与 Compute Pipeline](reference/pipeline.md)

### 高级 Render Pipeline component

- [RenderPipeline component 契约](reference/render-pipeline-contract.md)
- [Mesh](reference/mesh.md)
- [Material](reference/material.md)
- [Scene Snapshot](reference/scene-snapshot.md)
- [Render Pipeline](reference/render-pipeline.md)
- [Canvas Draw List](reference/canvas-draw-list.md)
- [Debug Draw List](reference/debug-draw-list.md)
- [Text Draw List](reference/text-draw-list.md)

### 窗口输出

- [Window 与 Input component 契约](reference/window-input-contract.md)
- [Window component](reference/window.md)
- [Input component](reference/input.md)
- [Surface](reference/surface.md)
- [Swapchain](reference/swapchain.md)
- [SDL3 与 GLFW 窗口接入](guides/window-library-integration.md)
- [SDL3 与 ImGui Integration](reference/third-party-integrations.md)

### 输入

- [Input 事件与状态值类型](reference/input.md)

## 架构与原理

- [总体架构、ABI 与渲染分层](concepts/architecture.md)
- [Vulkan Loader、Instance 与后端边界](concepts/vulkan-backend.md)
- [分阶段路线图](roadmap.md)

## 计划与历史

- [变更记录](../CHANGELOG.md)
- [开发计划索引](plans/README.md)
- [架构决策索引](decisions/README.md)
- [实施记录索引](records/README.md)
- 性能结果位于 [`benchmarks/results`](../benchmarks/results/README.md)。

计划描述未来或实施中的方案，不是当前公共能力的使用参考。已经验证的行为应以对应 API 文档和
仓库实现为准。
