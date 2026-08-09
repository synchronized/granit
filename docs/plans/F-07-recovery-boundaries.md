<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# F-07：窗口帧恢复边界

## 元数据

- 设计状态：已确认
- 实现状态：F-07A 已完成，F-07B/F-07C 待开始
- 路线图任务：F-07
- 优先级：P0
- 前置依赖：F-06
- 后续依赖：D-06、R-08C

## 目标

保证 acquire 之后的提前退出不会占住 Swapchain 图像，并为窗口最小化、Surface Lost 和
Device Lost 建立可判断、可恢复的边界。恢复策略由调用者驱动，Granit 负责保持内部状态一致，
不在 API 内部隐式创建窗口或无限重试。

## F-07A：Frame 取消与作用域回收（已完成）

`granit_frame_cancel` 消费尚未提交的 Frame 令牌。后端等待 image-available，执行把 acquired
图像转换到 `PRESENT_SRC_KHR` 的最小提交，signal render-finished 后 present，从而把图像合法
归还给 WSI。已经提交的 Frame 不能取消，应正常 present。

C++ `acquired_frame` 保存 Renderer 与 Swapchain 身份，并提供以下作用域保证：

- 已提交但未显式 present 的 Frame 在析构时尝试 present；
- 尚未提交的 Frame 在析构时尝试 cancel；
- 显式 `swapchain.cancel(frame)` 可返回错误及 `needs_recreate`，适用于需要可靠处理恢复状态的路径；
- 析构不抛异常且无法报告失败，因此不能替代关键路径上的显式错误处理。

取消成功、OUT_OF_DATE、SURFACE_LOST 或 DEVICE_LOST 都会消费 Frame 令牌；参数、句柄或内部准备
阶段出错时保留令牌，允许调用者诊断或重试。取消提交也占用 frames-in-flight 槽位，并在复用
前等待 Fence，不能绕过正常 GPU 完成语义。

## F-07B：窗口尺寸与重建状态（待开始）

- 明确零尺寸为“暂不可渲染”，而不是致命错误；调用者等待非零尺寸后再 recreate。
- acquire/present 的 OUT_OF_DATE 触发重建流程，SUBOPTIMAL 只设置 `needs_recreate`。
- 重建前必须先消费当前 Swapchain 的全部 Frame；重建成功后重新查询 backbuffer。
- 增加 resize、最小化、恢复和连续重建测试，不在零尺寸期间忙循环。

## F-07C：Surface Lost 与 Device Lost（待开始）

- Surface Lost：令对应 Surface/Swapchain 进入不可继续使用状态，要求重建平台 Surface 及
  Swapchain，不假设旧平台句柄仍然有效。
- Device Lost：Renderer 进入终止状态，后续 GPU 操作稳定返回 DEVICE_LOST；第一版不承诺在原
  Renderer 内重建设备，由调用者销毁并重新创建 Renderer 及其资源。
- 增加统一诊断信息，区分可重建的窗口状态与必须重建 Renderer 的设备故障。

## 验收标准

- acquire 后不录制、不提交即可显式取消，随后能够重建和继续帧循环。
- C++ Frame 离开作用域不会遗留阻止 Swapchain 重建的活动令牌。
- Validation Layer 下取消路径无 Semaphore、Fence、Layout 或对象生命周期错误。
- 零尺寸、OUT_OF_DATE、SUBOPTIMAL、SURFACE_LOST 和 DEVICE_LOST 的调用者动作均有明确文档和测试。

## F-07A 实际结果

- 新增 C API `granit_frame_cancel` 与 C++ `swapchain.cancel`。
- 取消通过真实 Queue submit 和 present 归还 acquired 图像。
- 帧槽支持没有用户 Recorder 的取消提交，并在复用时等待及回收序列号。
- Win32 Validation Layer 测试覆盖显式取消、析构自动取消及取消后的 Swapchain 重建。
