<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-62：Model Viewer 统一 Render Service

## 状态

**已完成，P1。** Desktop/Web 已通过同一个 `render_service` 使用 `render_runtime`，并分别保留
threaded 与 inline 执行策略。Pull Request #85 的 Linux、Windows、Emscripten 和浏览器验收均已通过。

## 背景与目标

当前 Desktop 的 `render_thread` 同时包装渲染操作和线程队列，Web 则直接组合
`inline_render_task_executor` 与 `render_runtime`。两端已经共享 GPU 实现和任务语义，但应用入口仍
调用不同层级的对象。

本阶段完成以下收敛：

- 新增共享 `render_service`，统一 Renderer、Presentation、Scene、Pipeline、质量、材质、帧执行和
  关闭接口；
- `render_service` 组合 `render_runtime` 与调用方选择的 `render_task_executor`；
- Desktop 的线程门面只保留异步帧队列、上传进度和完成回执；
- Web 通过 inline executor 使用同一个 `render_service`，不再直接操作 `render_runtime`；
- 保持普通帧可替换、控制任务不可丢弃以及浏览器主线程执行约束。

## 非目标

- 不强制 Desktop 与 Web 使用相同线程；
- 不把 Sample 私有服务加入 Granit SDK；
- 不改变 Render Pipeline、画面、加载进度或公共 API；
- 不把 Window 事件循环和资产 Fetch 纳入渲染服务。

## 实施顺序

1. **S-62A 服务契约（完成）**：定义共享操作、执行器所有权和直接执行边界。
2. **S-62B Desktop 迁移（完成）**：线程门面复用服务，只保留队列特有逻辑。
3. **S-62C Web 迁移（完成）**：以 inline executor 初始化服务并移除直接 Runtime 调用。
4. **S-62D 本地验收与文档（完成）**：Windows、Emscripten、Chrome 和文档验证通过。

## 约束

`render_service` 拥有 `render_runtime`，但不拥有 executor。executor 必须比服务活得更久。普通控制
操作通过 executor 串行执行；由 executor 回调进入的长上传任务使用明确的直接执行入口，避免在
threaded executor 工作线程内再次同步排队造成死锁。

## 验收

- Desktop/Web 应用层都通过 `render_service` 使用 GPU Runtime；
- `threaded_render_service` 不再直接拥有 `render_runtime`；
- Web 状态不再并列保存 Runtime 与 executor 回调；
- 既有 Desktop Model Viewer 测试、Vulkan 冒烟、正式/测试 Web 页面和 platform smoke 通过。

本地验收结果见
[S-62 Model Viewer 统一 Render Service 本地验收](../records/2026-09-24-s62-model-viewer-render-service-local-acceptance.md)。
