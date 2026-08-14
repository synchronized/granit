<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Granit 文档中心

这里是 Granit 使用指南、参考资料、架构说明和开发计划的统一入口。根 README 只负责项目介绍与
快速开始；具体事实以本页链接的对应文档为准。

## 推荐阅读顺序

### 使用核心 Renderer

1. [构建与安装](build.md)
2. [Renderer](renderer.md)
3. [资源类型](resource-types.md)
4. [Command Recorder](command-recorder.md)
5. [线程安全](thread-safety.md)

### 理解高级渲染层

1. [架构与 ABI](architecture.md)
2. [路线图](roadmap.md)
3. [Render Graph 边界](plans/P-06-render-graph-boundary.md)
4. [高级参考渲染套件](plans/H-07-reference-render-pipeline.md)

### 参与开发

1. [开发规范](development.md)
2. [项目文档规范](../DOCUMENTATION_GUIDE.md)
3. [开发计划索引](plans/README.md)
4. [第三方依赖](../3rd/README.md)

## 操作指南

- [构建、测试、安装与 CMake 集成](build.md)
- [示例程序及运行方式](examples.md)
- [同步批量上传](upload-batch.md)

## API 与行为参考

### 核心与数学

- [Renderer 生命周期与诊断](renderer.md)
- [公共数学值类型](math-types.md)
- [公开对象线程安全矩阵](thread-safety.md)

### GPU 资源

- [资源类型总览](resource-types.md)
- [Buffer](buffer.md)
- [Texture 与 Texture View](texture.md)
- [Sampler](sampler.md)
- [Render Target Attachment](render-target.md)

### 命令与 Pipeline

- [Command Recorder](command-recorder.md)
- [Shader Module](shader.md)
- [Graphics 与 Compute Pipeline](pipeline.md)

### 窗口输出

- [Surface](surface.md)
- [Swapchain](swapchain.md)

## 架构与原理

- [总体架构、ABI 与渲染分层](architecture.md)
- [Vulkan Loader、Instance 与后端边界](vulkan.md)
- [分阶段路线图](roadmap.md)

## 计划与历史

- [开发计划索引](plans/README.md)
- 性能结果位于 [`benchmarks/results`](../benchmarks/results/README.md)。

计划描述未来或实施中的方案，不是当前公共能力的使用参考。已经验证的行为应以对应 API 文档和
仓库实现为准。
