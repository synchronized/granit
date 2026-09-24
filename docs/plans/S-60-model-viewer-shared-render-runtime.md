<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-60：Model Viewer 共享渲染运行时

## 状态

**本地实施与浏览器验收完成，等待远端跨平台验收，P1。** 本计划延续 S-59 的统一任务语义，把
Desktop 原 `render_service` 与 Web 平台入口中
重复的 Renderer、Surface、Swapchain、Pipeline 和帧执行逻辑收敛到单一同步 `render_runtime`。
该实现只服务仓库 Sample，不进入 Granit 公共 SDK，也不改变 API/ABI。

## 背景与目标

Desktop 已通过 `render_service` 集中管理 GPU 对象，但该类型同时拥有专用线程执行器；Web 则在
`application.cpp` 中直接维护同一批对象。现有 `render_task_executor` 已统一“任务在哪里执行”，仍
缺少统一的“任务具体执行什么”。

本阶段完成以下收敛：

- 提取同步、后端无关的 `render_runtime`，统一 Renderer、Surface、Swapchain、Pipeline 与帧资源；
- 让 Desktop 通过 threaded executor、Web 通过 inline executor 调用同一运行时；
- 统一 Scene 上传、质量修改、Swapchain 重建、帧执行、统计与资源释放；
- 删除 Desktop/Web 中重复的 GPU 生命周期实现和过渡 `render_service`。

## 非目标

- 不把 Sample 私有运行时安装到 SDK，不新增公共 Renderer 调度 API；
- 不统一 Desktop 与浏览器的窗口循环、资产来源、Asyncify 或自动化导出；
- 不在运行时内部创建线程，线程位置只由 `render_task_executor` 决定；
- 不借此次重构修改 PBR、材质、光照或画面结果。

## 已确认决策

`render_runtime` 是一个同步具体类，不使用平台继承层。它拥有 Renderer、Surface、Swapchain、Render
Pipeline、Canvas、字体和帧资源，并接收 `application_core` 以执行上传和质量变更。所有方法只能在
选定执行器的消费线程调用。

Desktop 平台壳持有 threaded executor；Web 平台壳持有 inline executor。Surface 创建统一接收
`granit::window`，恢复事件由平台壳触发。WebGPU 异步 Pipeline 预热以 begin/poll 状态表达，由 Web
循环负责让出浏览器事件循环，不能把 Asyncify 写入共享运行时。

## 实施顺序

1. **S-60A 契约基线（已完成）**：已登记所有权、线程、异步和平台边界。
2. **S-60B 同步运行时（已完成）**：已从 Desktop 原 `render_service` 提取 GPU 状态与同步操作。
3. **S-60C Desktop 迁移（已完成）**：Desktop 以 threaded executor 调用共享运行时；剩余队列、
   取消和回执门面已改名为 `render_thread`。
4. **S-60D Web 迁移（已完成）**：Web 以 inline executor 调用共享运行时，只保留 Fetch、Asyncify、
   Pipeline 预热与浏览器导出。
5. **S-60E 清理与验收（本地完成）**：已删除 `desktop/render_service.*`，更新 Guide，并完成 Windows、
   Emscripten、Chrome 与文档验证。详细结果见
   [S-60 本地验收记录](../records/2026-09-24-s60-shared-render-runtime-local-acceptance.md)。

## 测试与验收

- 同一运行时覆盖初始化、上传、首帧、连续帧、质量切换、Resize、重建和重复关闭；
- Desktop 的渲染线程所有权、帧替换、控制任务顺序和 Surface 恢复保持通过；
- Web 的 Fetch、Pipeline 预热、输入、Resize、取消回滚、错误诊断和资源归零保持通过；
- 平台入口不再直接拥有或销毁 Renderer、Surface、Swapchain 与 Render Pipeline；
- Windows、Emscripten、Chrome 行为验收、文档检查和 `git diff --check` 通过。

## 风险与未决问题

- Renderer 初始化在 Web 上异步完成，运行时必须保留明确的 pending 状态；
- Desktop Window 与 Surface 恢复有线程交接约束，共享类型不能跨线程读取 Window 可变状态；
- Pipeline 验收对象目前包含浏览器专项检查，迁移时应区分正式运行资源与测试探针；
- `application_core` 当前拥有 GPU Scene，运行时只能在执行器线程调用其 GPU 方法。
