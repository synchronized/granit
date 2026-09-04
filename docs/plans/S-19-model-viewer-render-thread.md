<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-19：Model Viewer 渲染线程

## 状态

S-19A 至 S-19E 已完成。当前只修改示例私有代码，不增加公共 API；
桌面使用有界线程执行器，浏览器保持同步执行器，二者消费同一种不可变帧包。

## 背景与目标

桌面 Model Viewer 当前在 SDL 主线程完成事件、UI、场景更新、GPU 上传、录制、提交和 Present。
模型上传或帧槽等待可能阻塞窗口消息处理。目标是让桌面 Vulkan 的 GPU 工作由单独渲染线程拥有，
主线程只生成有界、不可变的帧输入，同时保持浏览器 Emscripten WebGPU 使用同一帧包的同步执行。

## 非目标

- 不公开通用 Render Thread API。
- 不让 Granit 接管应用线程模型。
- 不在首版启用 Emscripten Pthreads、Web Worker 或 OffscreenCanvas。
- 不允许多个线程并发调用同一 Render Pipeline、Swapchain 或 GPU Scene。

## 已确认决策

- `frame_packet` 必须拥有 Scene Snapshot、环境描述和 Draw Binding 副本，不借用下一帧可变状态。
- 桌面渲染线程独占 Swapchain、Frame Context、Pipeline、GPU Scene 和 GPU 资源销毁。
- 主线程深拷贝 ImGui Draw Data；渲染线程使用按帧槽独立的 Canvas，不能复用正在提交的对象。
- 队列最多保留三个 Frame Packet。满载时删除最旧的尚未执行帧并保留最新帧，不无限累积输入
  延迟；资源与控制命令的相对顺序不变。
- 资源与控制命令不可被帧替换；命令队列达到固定上限时返回 `not_ready`，由生产者决定重试。
- GPU Scene 上传、Pipeline 创建、Swapchain Resize、质量切换、材质更新和正常关闭资源回收均作为
  不可丢弃命令在常驻渲染线程执行。
- Renderer 与首个 Surface/Swapchain 的平台引导发生在渲染线程启动前；Surface Lost 恢复需要访问
  SDL Window，主线程必须先排空队列再执行，不能与渲染线程并发。
- 性能面板和性能报告记录渲染队列等待时间、高水位及累计替换帧数；是否改用无锁环形队列以
  实测结果为依据。
- 普通帧异步入队；Resize、Surface Lost、质量切换和资源修改先排空队列，再由主线程串行处理。
- 关闭先排空帧队列，再停止并 Join 工作线程，最后按既有逆序销毁 GPU 资源。
- glTF 与图片解析可在 Worker 执行；创建 GPU 对象和 Upload Batch 仍由渲染线程完成。
- 浏览器使用 `inline_frame_executor`，桌面 Vulkan 使用 `threaded_frame_executor`。

## 实施顺序

1. **S-19A：不可变帧包（已完成）**。移除 Render Desc 对 Core/GPU Scene 可变数组的借用，并
   验证移动后描述仍指向帧包自身。
2. **S-19B：执行器边界（已完成）**。增加私有 `frame_executor` 与同步实现；桌面和浏览器均把
   Acquire、Render 和 Present 收敛到执行回调，同时保持原有同步提交行为。
3. **S-19C：UI 与资源槽（已完成）**。把 ImGui 输出复制为纯 CPU Canvas 数据，并为三个在途帧
   准备轮换使用的独立 Canvas。
4. **S-19D：桌面线程执行器（已完成）**。桌面已使用有界三帧队列、满载替换和完成回执；
   Resize、恢复、质量与材质修改使用排空边界，关闭顺序确定。
5. **S-19E：上传拆分（已完成）**。Worker 只返回自有 CPU Scene、环境字节和诊断，不修改
   Application Core；顶点与索引打包、Primitive/Draw 展开及纹理/采样器计划也已迁入 Worker。
   桌面有界队列区分可替换帧与不可丢弃命令，GPU 上传通过命令交给常驻渲染线程；主线程同期
   只处理 SDL 事件。主线程预先捕获自有的加载 UI 帧，渲染线程在各资源边界穿插显示 GPU 上传
   进度，不跨线程调用 ImGui 或 SDL。

## 测试与验收

- 覆盖 Frame Packet 自有数据、移动语义和 Core 下一帧修改不影响已提交包。
- 覆盖队列 FIFO、满载替换、停止生产、排空、失败回执和重复关闭。
- 拖动、Resize、材质修改和质量切换期间不出现数据竞争、悬空句柄或窗口未响应。
- 正常关闭时先在渲染线程逆序释放 GPU 资源和 Renderer，再停止并 Join 执行器。
- 桌面 Vulkan Smoke 与性能采样通过；线程模式相对同步模式不增加持续输入延迟。
- 浏览器构建和 WebGPU 自动测试保持通过。

## 风险与未决问题

- `frame_packet` 每帧复制 Draw Binding 会增加 CPU 成本；S-19D 前通过基准决定是否改为共享不可变
  Binding 集合。
- GPU 提交线程不能消除 Present 或驱动内部阻塞，只负责把阻塞从窗口消息线程移开。
- 公共 API 只有在至少两个非示例 Consumer 需要相同语义后才另行设计。
- 当前互斥量与条件变量实现是可测量基线；只有队列等待成为稳定瓶颈时才替换内部实现。
