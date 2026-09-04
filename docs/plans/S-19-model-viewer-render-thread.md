<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-19：Model Viewer 渲染线程

## 状态

设计已确认，S-19A 至 S-19C 已完成，S-19D 实现中。当前只修改示例私有代码，不增加公共 API；
桌面与浏览器均通过同步执行器消费同一种不可变帧包，桌面 UI 已不再借用 ImGui 当前帧状态。

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
- 队列最多保留三个 Frame Packet。满载时替换尚未开始的旧包，不无限累积输入延迟。
- Resize、Surface Lost、Device Lost、质量切换和关闭通过显式命令进入同一队列。
- glTF 与图片解析可在 Worker 执行；创建 GPU 对象和 Upload Batch 仍由渲染线程完成。
- 浏览器使用 `inline_frame_executor`，桌面 Vulkan 使用 `threaded_frame_executor`。

## 实施顺序

1. **S-19A：不可变帧包（已完成）**。移除 Render Desc 对 Core/GPU Scene 可变数组的借用，并
   验证移动后描述仍指向帧包自身。
2. **S-19B：执行器边界（已完成）**。增加私有 `frame_executor` 与同步实现；桌面和浏览器均把
   Acquire、Render 和 Present 收敛到执行回调，同时保持原有同步提交行为。
3. **S-19C：UI 与资源槽（已完成）**。把 ImGui 输出复制为纯 CPU Canvas 数据，并为三个在途帧
   准备轮换使用的独立 Canvas。
4. **S-19D：桌面线程执行器（实现中）**。有界三帧队列、满载替换、完成回执、排空和确定性
   关闭已完成单元验证；下一步接入桌面主循环并加入 Resize 与恢复命令。
5. **S-19E：上传拆分**。Worker 只做 CPU 解码，渲染线程分批创建并提交 GPU 资源。

## 测试与验收

- 覆盖 Frame Packet 自有数据、移动语义和 Core 下一帧修改不影响已提交包。
- 覆盖队列 FIFO、满载替换、停止生产、排空、失败回执和重复关闭。
- 拖动、Resize、材质修改和质量切换期间不出现数据竞争、悬空句柄或窗口未响应。
- 桌面 Vulkan Smoke 与性能采样通过；线程模式相对同步模式不增加持续输入延迟。
- 浏览器构建和 WebGPU 自动测试保持通过。

## 风险与未决问题

- `frame_packet` 每帧复制 Draw Binding 会增加 CPU 成本；S-19D 前通过基准决定是否改为共享不可变
  Binding 集合。
- GPU 提交线程不能消除 Present 或驱动内部阻塞，只负责把阻塞从窗口消息线程移开。
- 公共 API 只有在至少两个非示例 Consumer 需要相同语义后才另行设计。
