<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Granit 文档中心

这里是 Granit 教程、操作指南、参考资料、架构说明和开发计划的统一入口。根 README 只负责项目
介绍与快速开始；具体事实以本页链接的对应文档为准。教程用于从零完成完整目标，指南用于解决已知
任务，参考文档用于查询当前 API、格式和行为契约。

## 推荐阅读顺序

### 第一次使用参考渲染管线

1. [创建第一个 Renderer](tutorials/01-first-renderer.md)
2. [Render Pipeline 离屏渲染教程](tutorials/render-pipeline-offscreen.md)
3. [示例程序及运行方式](guides/examples.md)
4. [Render Pipeline](reference/render-pipeline.md)

### 使用核心 Renderer

1. [构建与安装](guides/build.md)
2. [Renderer](reference/renderer.md)
3. [资源类型](reference/resource-types.md)
4. [Command Recorder](reference/command-recorder.md)
5. [Frame Context](reference/frame-context.md)
6. [线程安全](reference/thread-safety.md)

### 理解高级渲染层

1. [架构与 ABI](concepts/architecture.md)
2. [Render Pipeline](reference/render-pipeline.md)
3. [Material](reference/material.md)
4. [Scene Snapshot](reference/scene-snapshot.md)
5. [运行跨后端 Model Viewer](tutorials/model-viewer.md)

### 参与开发

1. [开发规范](guides/development.md)
2. [项目文档规范](../DOCUMENTATION_GUIDE.md)
3. [路线图](roadmap.md)
4. [0.26.0 当前计划](plans/S-45-0.26.0-documentation-convergence.md)
5. [开发计划与完成历史](plans/README.md)
6. [第三方依赖](../3rd/README.md)

## 操作指南

指南面向已经了解目标的使用者，重点是构建、集成、迁移、验证和排错步骤；不承担完整入门教程或
逐项 API 定义。

- [构建、测试、安装与 CMake 集成](guides/build.md)
- [发布验收](guides/release.md)
- [版本迁移指南索引](guides/migrations.md)
- [从 0.24 迁移到 0.25](guides/migrate-0.24-to-0.25.md)
- [示例程序及运行方式](guides/examples.md)
- [运行浏览器 WebGPU 平台 Smoke](guides/webgpu-browser-example.md)
- [异步回读与 Pipeline 预热](guides/async-readback-and-pipeline-warmup.md)
- [纹理同步回读](guides/texture-readback.md)

## 教程

教程从前置条件开始，连续完成一个可验证的目标；其中的 API 细节以对应参考文档为准。

- [01：创建第一个 Renderer](tutorials/01-first-renderer.md)
- [使用 Render Pipeline 完成第一次离屏渲染](tutorials/render-pipeline-offscreen.md)
- [运行跨后端模型查看器](tutorials/model-viewer.md)

## API 与行为参考

参考文档是当前行为、格式、所有权和限制的权威来源，通常不提供从零开始的完整操作流程。

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
- [Texture Asset Manifest](reference/texture-asset.md)
- [Sampler](reference/sampler.md)
- [Render Target Attachment](reference/render-target.md)

### 命令与 Pipeline

- [AssetTools SDK](reference/asset-tools.md)
- [Shader 工具链包清单](reference/shader-toolchain-package.md)
- [Command Recorder](reference/command-recorder.md)
- [Frame Context](reference/frame-context.md)
- [Timestamp Query](reference/timestamp-query.md)
- [Shader Module](reference/shader.md)
- [Shader Library](reference/shader-library.md)
- [Graphics 与 Compute Pipeline](reference/pipeline.md)
- [Upload Batch](reference/upload-batch.md)

### 高级 Render Pipeline component

- [RenderPipeline component 契约](reference/render-pipeline-contract.md)
- [Environment Map 与 GRENV v3](reference/environment-map.md)
- [Mesh](reference/mesh.md)
- [Material](reference/material.md)
- [Scene Snapshot](reference/scene-snapshot.md)
- [Render Pipeline](reference/render-pipeline.md)
- [Canvas Draw List](reference/canvas-draw-list.md)
- [Debug Draw List](reference/debug-draw-list.md)
- [Text Draw List](reference/text-draw-list.md)

### 窗口输出

- [Window 与输入契约](reference/window-input-contract.md)
- [Window component](reference/window.md)
- [Window 输入](reference/input.md)
- [Surface](reference/surface.md)
- [Swapchain](reference/swapchain.md)
- [SDL3 与 GLFW 窗口接入](guides/window-library-integration.md)
- [SDL3 与 ImGui Integration](reference/third-party-integrations.md)

## 架构与原理

- [总体架构、ABI 与渲染分层](concepts/architecture.md)
- [Vulkan Loader、Instance 与后端边界](concepts/vulkan-backend.md)
- [第三方 UI 与字体适配边界](concepts/third-party-ui-adapter-boundary.md)
- [分阶段路线图](roadmap.md)

## 计划与历史

- [变更记录](../CHANGELOG.md)
- [开发计划索引](plans/README.md)
- [已完成计划索引](plans/completed.md)
- [架构决策索引](decisions/README.md)
- [实施记录索引](records/README.md)
- [版本验收模板](templates/version-acceptance.md)
- 性能结果位于 [`benchmarks/results`](../benchmarks/results/README.md)。

计划描述未来或实施中的方案，不是当前公共能力的使用参考。已经验证的行为应以对应 API 文档和
仓库实现为准。
