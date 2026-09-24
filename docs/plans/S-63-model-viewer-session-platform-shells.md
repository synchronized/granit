<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-63：Model Viewer Session 与平台壳收敛

## 状态

**实施中，P1。** 本计划把 Desktop/Web 重复的启动、加载、UI 帧构造和呈现恢复收进共享 Session，
让平台壳只保留真实的调度与宿主差异。

## 背景与目标

S-58～S-62 已统一资产、Core、ImGui、Render Runtime 和 Render Service，但两个平台入口仍分别维护
接近八百行的应用编排。Renderer ready、模型加载、预览纹理、质量切换、帧构造和 Present 恢复的
调用顺序仍可能漂移。

本阶段完成以下收敛：

- 生产代码优先使用 C++ RAII 和强类型引用，把 C ABI 验收回调隔离到测试目标；
- 统一材质纹理预览和 Present 结果恢复策略；
- 将 CPU 生命周期、模型与环境资产、质量配置和帧构造组合为共享 `viewer_session`；
- 建立唯一的 `viewer_application` 状态机，Desktop/Web 入口只生成配置参数；
- 统一 inline/threaded 帧与控制任务完成协议，删除 Desktop 专用 Render Service 门面；
- 将 Pipeline 准备和 CPU/GPU 异步任务提升为跨平台协议，把线程、Asyncify 和后端差异留在实现内。

## 非目标

- 不强制 Desktop 与 Web 使用相同线程；
- 不把 Sample 私有 Session 或 Application Host 加入 Granit SDK；
- 不把浏览器测试 C ABI 混入正式运行路径；
- 不统一 Desktop 性能报告、Web Asyncify 或平台资产 Source；
- 不改变公共 API、渲染效果和资产格式。

## 已确认边界

`viewer_session` 不拥有 Window、事件循环或执行线程。它只消费统一输入、推进 CPU/资产状态并产生
平台无关的帧包与下一步动作。`render_service` 继续拥有 GPU 资源，executor 继续决定调用线程。
平台壳负责执行 Session 动作并把结果送回 Session。

测试专用 `pipeline_validation` 保留直接 C API 调用，用于覆盖 C ABI；正式 Pipeline Warmup 改用
C++ RAII。Surface 和主循环的具体实现保持在平台壳，恢复决策使用共用分类。

## 实施顺序

1. **S-63A 强类型生产路径与共享小组件（完成）**：统一纹理预览，迁移 Pipeline Warmup 到
   C++ RAII，把 Web 原生句柄回调移入测试专用接口。
2. **S-63B Present 恢复（完成）**：共享结果分类和恢复状态，补齐 Web 对 `needs_recreate`、
   `out_of_date` 与 `surface_lost` 的处理。
3. **S-63C Viewer Session（完成）**：用拥有型 `viewer_session` 组合 Application Core 与模型加载，
   统一 Renderer 阶段、CPU Scene 交接、失败、取消、重置及 GPU 操作入口；Render Runtime 只借用
   Session，平台入口不再并列维护三套生命周期对象。
4. **S-63D 执行协议（完成）**：已对称化 inline/threaded 的帧与控制任务提交、容量和完成回执，
   GPU 上传通过共用 Render Service 提交，并删除 `threaded_render_service`。
5. **S-63E 统一异步准备（完成）**：共享 Pipeline Prepare 与 CPU Scene Prepare Task，GPU 上传
   统一通过 Render Service 的控制任务协议执行；线程和浏览器主循环差异留在任务实现内。
6. **S-63F 唯一 Application（待开始）**：实现一个 `viewer_application` 状态机，Desktop/Web
   入口只负责解析参数并构造描述。
7. **S-63G 验收与文档（待开始）**：验证 Windows、Emscripten、Chrome、Linux 和文档。

## 验收

- Desktop/Web 不再分别维护 Viewer 启动和单帧业务流程；
- 正式 Model Viewer 路径不依赖裸 C 句柄，测试 C ABI 路径保持独立；
- 两端对 Present 恢复、质量切换、材质修改和纹理预览具有相同语义；
- Desktop 继续使用专用渲染线程，Web 继续在浏览器主线程执行；
- Windows、Emscripten、Chrome 正式/测试页面、Platform Smoke 和文档检查通过。

## 风险

共享 Session 不能阻塞浏览器事件循环，也不能在 Desktop 渲染线程中递归同步排队。迁移采用可独立
验证的小阶段，先收敛纯数据与强类型接口，再改变宿主和执行协议。
