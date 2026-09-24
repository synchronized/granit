<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 路线图

本路线图只记录当前阶段、正在实施的任务、暂缓候选和长期方向，不构成版本或发布日期承诺。
已发布版本的变化见 [Changelog](../CHANGELOG.md)，详细设计见[计划索引](plans/README.md)，实际执行
证据见[实施记录](records/README.md)。

## 当前阶段

Granit 已形成桌面 Vulkan、浏览器 WebGPU、C11 ABI、C++20 RAII、Window、RenderPipeline 与
AssetTools 的完整闭环。当前仍处于 0.x，下一阶段优先提高接口、文档和发布契约的可信度，再根据
真实使用证据决定新的公共能力。

| 能力域 | 状态 | 当前边界 |
|---|---|---|
| Core 与 ABI | 可用，未冻结 | 结果码、强类型句柄、C/C++ 包装、诊断与安装 Consumer 已覆盖 |
| Vulkan 桌面后端 | 可用 | Windows Win32 与 Linux XCB/Wayland，资源、命令、提交和呈现完整 |
| WebGPU 浏览器后端 | 可用 | Emscripten 浏览器资源、PBR、异步操作、Timestamp 与呈现闭环 |
| GPU 资源与 Pipeline | 可用 | Buffer、Texture、Sampler、Shader Library、Graphics/Compute 与传输 |
| Frame 与 Swapchain | 可用 | Frame Context、在途槽、恢复边界、取消、Present 与延迟回收 |
| RenderPipeline | 可用，未冻结 | Scene Snapshot、Material、Forward PBR、Shadow、IBL、UI 与后处理 |
| Window 与 Input | 可用，未冻结 | 单一 Window System 管理窗口、事件、输入状态和直接 Surface 创建 |
| AssetTools | 实验性 | HLSL-first Shader Library、Material、Texture、Environment 构建与检查 |
| SDK 与发布 | 可用 | Windows/Linux shared SDK、静态源码构建、安装审计与不可变候选晋级 |

各能力的准确使用方式和限制以 [Reference](README.md#api-与行为参考) 为准。

## 当前计划

### S-63：Model Viewer Session 与平台壳收敛

**状态：实施中，P1。**

[S-63](plans/S-63-model-viewer-session-platform-shells.md) 将 Desktop/Web 重复的启动、加载、UI 帧
构造和呈现恢复收进共享 Session，并继续保留 threaded/inline 与宿主平台差异。

### S-62：Model Viewer 统一 Render Service

**状态：本地实施与浏览器验收完成，等待远端跨平台验收，P1。**

[S-62](plans/S-62-model-viewer-render-service.md) 统一 Desktop/Web 对 Render Runtime 的调用门面，
并把线程队列、帧替换和完成回执收窄为 Desktop 执行策略。

### S-61：Model Viewer 共享 ImGui 与浏览器测试边界

**状态：本地实施与浏览器验收完成，等待远端跨平台验收，P1。**

[S-61](plans/S-61-model-viewer-shared-imgui-browser-tests.md) 让 Desktop/Web 使用同一套 ImGui Viewer
面板，并把浏览器测试导出和 Pipeline C API 探针移出正式运行路径。

### S-60：Model Viewer 共享渲染运行时

**状态：本地实施与浏览器验收完成，等待远端跨平台验收，P1。**

[S-60](plans/S-60-model-viewer-shared-render-runtime.md) 将 Desktop/Web 重复的 Renderer、Surface、
Swapchain、Pipeline 和帧执行逻辑提取为同步 `render_runtime`，并由 threaded/inline executor 选择
执行位置。

### S-59：Model Viewer 渲染任务运行时

**状态：本地实施与浏览器验收完成，等待远端跨平台验收，P1。**

[S-59](plans/S-59-model-viewer-render-task-runtime.md) 将帧执行器提升为拥有型强类型渲染任务执行器，
以 threaded/inline 两种策略统一 Desktop/Web 的 GPU 任务语义，并在其上收敛 Viewer 生命周期状态机。

### S-58：统一 Example Asset System

**状态：实施中，P1。**

[S-58](plans/S-58-unified-example-asset-system.md) 将资产身份、来源与读取调度拆开，以逻辑 Mount 和
统一请求隐藏 Desktop 目录、Emscripten 预加载文件系统与 Fetch 差异，并统一 Application Host、
Tutorial、glTF 和 Model Viewer 的资产入口。

### S-57：Model Viewer 运行时架构

**状态：实施完成，等待远端 Linux 验收，P1。**

[S-57](plans/S-57-model-viewer-runtime-architecture.md) 在保持 Desktop 渲染线程、Web inline/Asyncify
和现有目录入口的前提下，拆分 Viewer Core 输出、执行包、桌面渲染服务与浏览器验收接口，使两个
千行平台入口回到清晰的组合职责。

### S-56：Window Target 与 SDL3 后端

**状态：实施完成，等待远端 Linux 验收，P1。**

[S-56](plans/S-56-window-target-and-sdl3-backend.md) 分离 Window Backend 与 Window Target，先让
原生 Emscripten 支持多 Canvas，再以可选 SDL3 后端统一桌面与浏览器的 Window API。Windows、
Emscripten、浏览器和安装 Consumer 本地验收已经通过，等待远端 Linux XCB/Wayland 与 SDL3 矩阵。

### S-54：0.30.0 特性教程与 Example Application

**状态：实施完成，等待远端验收，P1。**

[S-54](plans/S-54-0.30.0-feature-tutorial-framework.md) 将以仓库私有 Application 统一示例生命周期，
把线性教程压缩为 Cube 与 PBR Assets 两个特性入口，将完整 Model Viewer 移回 Samples，并删除
重复的最小 Model Loading 应用。
Windows、Emscripten 与浏览器本地验收已经通过，等待远端 Linux 与 Release 矩阵。

### S-53：0.30.0 文档、教程与发布面收敛

**状态：实施完成，等待远端验收，P1。**

[S-53](plans/S-53-0.30.0-documentation-tutorial-release.md) 将明确 Frame Context 与 Command Recorder
的分层，收敛文档和十章教程入口，并让正式 Release 只提供推荐的 shared SDK。静态源码构建和 CI
验证继续保留。

## 最近完成

### S-52：0.29.0 版本收口与发布验收

**状态：已完成，P1。**

[S-52](plans/S-52-0.29.0-release-acceptance.md) 已完成迁移说明、跨平台测试、四套 SDK 候选包、
SHA-256 校验与公开下载复验，并发布 `v0.29.0`。

### S-51：0.29.0 Shader Library 逻辑名称

**状态：已完成，P1。**

[S-51](plans/S-51-0.29.0-shader-library-logical-names.md) 已把 Library 与 Shader 逻辑名称写入
`.grshlib`，运行时和 Material Builder 直接消费该归档，并删除独立索引和生成内容 ID include。

### S-50：0.29.0 Model Viewer 教程迁移

**状态：已完成，P1。**

[S-50](plans/S-50-0.29.0-model-viewer-tutorial-migration.md) 已新增 09 Model Loading，并把跨后端
Model Viewer 收敛为 Tutorial 10。桌面、Web 与离屏入口已经切换，重复 Sample 已删除。

### S-48：线性入门教程与配套示例

**状态：已完成，P1。**

[S-48](plans/S-48-0.28.0-linear-tutorial-series.md) 已交付从 Window、Triangle、Texture、Camera、
Mesh、Material/Lighting、Render Pipeline 到 ImGui 的八章连续教程。全部章节具有配套源码与桌面
自动验证，Window 与 ImGui 章节同时覆盖浏览器 WebGPU；普通 C++ 使用路径已完成强类型审计。

### S-49：0.28.0 Window 跨平台可选主循环

**状态：已完成，P1。**

[S-49](plans/S-49-0.28.0-window-loop.md) 已在非阻塞 Window 事件泵之上增加可选托管 Loop。桌面和
Emscripten 可复用同一个应用 Tick，Loop 不持有或推进 Renderer，也不接管资源或引擎任务系统。

### S-47：Window 跨平台入口收敛

**状态：已完成，P1。**

[S-47](plans/S-47-window-platform-convergence.md) 已让 Emscripten 与桌面平台共享 Window System、
窗口和输入事件、状态查询及 Surface 创建流程。浏览器安装包现在导出 Window component，Model
Viewer 不再维护私有 DOM 输入和 Canvas Surface 生命周期。

### S-45：0.26.0 文档一致性与历史收敛

**状态：已完成，P1。**

[S-45](plans/S-45-0.26.0-documentation-convergence.md) 已修正当前文档与 0.25.0 实现之间的事实
漂移，收敛 Concept、Reference、Roadmap、Plan 与 Record 职责，并扩展确定性文档检查。本任务未
修改公共 API、ABI、资产格式或运行时行为。

完成内容：

1. 修正版本、组件和发布状态。
2. 让 Concept 与 Reference 反映当前后端和资源能力。
3. 压缩路线图、计划索引和长实施记录。
4. 增加版本身份与已删除 component 检查，完成 Documentation 验收。

## 暂缓与重新评估条件

以下方向没有进入当前版本。只有满足对应证据后才建立新的实施计划。

| 方向 | 当前决定 | 恢复条件 |
|---|---|---|
| [Bindless Resource Table](plans/D-09-bindless-resource-table.md) | 暂缓 | 真实材质绑定压力证明传统 Bind Group 成为瓶颈 |
| [透明 PBR](plans/H-09B-transparent-pbr-correctness.md) | 暂缓 | 产品需要正确折射、排序或大量透明 PBR 材质 |
| Clustered Forward | 暂缓 | 多光源负载稳定超过当前 Forward 路径预算 |
| Cascaded Shadow Maps | 暂缓 | 大尺度室外场景证明单方向光 Shadow Map 不足 |
| 公共 glTF/Scene SDK | 暂缓 | 至少第二个独立 Consumer 需要复用示例私有加载器 |
| 公共执行器或线程池 | 暂缓 | 至少第二个模块需要相同调度与取消契约 |
| Android | 待规划 | 明确 NDK、Surface、生命周期、输入和 CI 设备矩阵 |
| 稳定 ABI | 待决策 | 稳定 component 范围和兼容门槛全部满足 |

这些方向互不构成前置依赖。测量或 Consumer 证据不足时继续使用当前实现，不提前扩展公共 ABI。

## 长期方向

- 保持 Renderer、RenderPipeline、Window、AssetTools 与第三方 Integration 的单向依赖。
- 继续以 Vulkan 和浏览器 WebGPU 的共同语义为公共契约，不为表面对称模拟不安全能力。
- 用格式版本管理持久化资产兼容，用 component 分别声明 API/ABI 稳定等级。
- 优先改进真实上游接入、诊断、性能测量和发布复现，再增加渲染特性。
- 原生后端互操作若出现明确需求，作为显式不稳定高级接口设计，不污染基础 API。

## 历史入口

- [Changelog](../CHANGELOG.md)：面向使用者的逐版本变化和迁移影响。
- [迁移指南](guides/migrate-0.28-to-0.29.md)：最近一次破坏性版本迁移。
- [计划索引](plans/README.md)：当前与暂缓任务；[完成计划索引](plans/completed.md)保存已验收计划。
- [实施记录](records/README.md)：跨平台、性能和发布验收证据。

扩大公共 API 前应先更新对应 Plan 和本路线图状态，并明确验证与退出条件。
