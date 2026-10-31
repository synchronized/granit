<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# ADR-008：公共 Render Graph 作为独立 Renderer 扩展层

- 状态：提议
- 日期：2026-10-31

## 背景

Granit 已有内部 Render Graph 原型，但它位于 `src/`，服务于参考 RenderPipeline。外部引擎需要
复用 Renderer 的资源、命令和同步能力，同时保留自己的 Scene、Material、可见性和 Pass 组织。
直接公开内部头文件会泄漏实现容器、资源状态和后端细节，也会强制外部引擎依赖参考管线。

## 决策

将公共 Render Graph 设计为独立的 `RenderGraph` component，只依赖核心 Renderer。它提供公共
C ABI 和 C++ RAII 包装，Graph 通过资源访问用途建立依赖和生命周期；Renderer 负责实际后端状态
转换、命令执行和 GPU 延迟回收。首期不公开 Vulkan 类型、不自动合并 Subpass、不接管 Pipeline
创建和资产加载。

导入资源采用借用契约，临时资源由 Graph 创建并在提交后交给 Renderer 延迟回收。Pass 回调只能
在当前录制期间查询资源和使用 Recorder；回调之外保存资源句柄不属于公共契约。

## 影响

- toy-engine 可以逐步迁移静态 Mesh、阴影和后处理，而不迁移完整参考材质系统；
- 公共 API 必须覆盖 C ABI、安装导出、错误诊断和生命周期测试，不能只包装现有 C++ 原型；
- 现有内部 Graph 实现需要拆分，并修正当前提交前销毁瞬态资源的问题；
- Subpass 用户需要改为独立 Pass 和 sampled Texture 路径，首期不承诺绘制代码零改动迁移；
- RenderPipeline 继续作为可选高级组件，不反向依赖公共 Graph 的业务语义。

## 替代方案

1. 直接公开 `src/render_graph`：改动最小，但会泄漏内部头文件、C++ 容器和不稳定生命周期，拒绝。
2. 只提供 RenderPipeline 扩展点：无法满足外部引擎自定义 Pass 和资源调度需求，拒绝。
3. 只提供 C++ API：无法满足现有 C ABI、安装 SDK 和其他语言绑定边界，拒绝。
