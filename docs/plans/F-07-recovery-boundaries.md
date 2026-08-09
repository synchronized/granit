<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# F-07：窗口帧恢复边界

## 元数据

- 设计状态：已确认
- 实现状态：已完成
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

## F-07B：窗口尺寸与重建状态（已完成）

- 零尺寸返回 `GRANIT_ERROR_NOT_READY`，表示“暂不可渲染”；调用者等待非零尺寸后再 recreate。
- acquire/present 的 OUT_OF_DATE 触发重建流程，SUBOPTIMAL 只设置 `needs_recreate`。
- 重建前必须先消费当前 Swapchain 的全部 Frame；重建成功后重新查询 backbuffer。
- 零尺寸不会进入 Vulkan 重建，也不会使旧 Swapchain 和 backbuffer 句柄失效。

## F-07C：Surface Lost 与 Device Lost

### F-07C-A：窗口帧终止状态（已完成）

- Surface Lost 会粘滞在对应 Swapchain 上，后续 backbuffer、acquire 和 recreate 稳定返回
  SURFACE_LOST；调用者必须销毁并重建平台 Surface 和 Swapchain。
- Device Lost 会粘滞在 Renderer 内，后续 Swapchain 创建、重建和帧操作稳定返回 DEVICE_LOST，
  不再继续调用 Vulkan WSI。
- present/cancel 即使返回 SURFACE_LOST 或 DEVICE_LOST 也会消费 Frame 令牌，避免终止路径遗留
  活动 Frame 阻止清理。

### F-07C-B：Renderer 全局终止（已完成）

- DEVICE_LOST 门禁覆盖 Buffer、Texture、Sampler、Recorder、离屏提交和等待等 GPU 操作。
- 内部粘滞状态组件允许独立注入结果并验证终止语义，不依赖真实驱动故障。
- 第一版不在原 Renderer 内重建设备；调用者销毁并重新创建 Renderer 及其全部资源。
- 日志与诊断回调归入 S-02，不阻塞本任务完成。

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

## F-07B 实际结果

- 新增公共结果码 `GRANIT_ERROR_NOT_READY` 与 C++ `result::not_ready`。
- 创建 Swapchain 时零尺寸仍是无效参数；已有 Swapchain 的零尺寸重建返回 NOT_READY。
- Vulkan `VK_NOT_READY` 和 `VK_TIMEOUT` 统一映射为 NOT_READY。
- Win32 测试验证零尺寸重建不会替换或使现有 backbuffer 句柄失效。
- OUT_OF_DATE 继续通过结果码触发重建，SUBOPTIMAL 继续只通过 `needs_recreate` 提示重建。

## F-07C-A 实际结果

- Renderer 记录粘滞的 Device Lost 状态，并为 WSI 路径提供统一结果观察入口。
- Swapchain Registry 记录粘滞的 Surface Lost 状态，拒绝继续获取旧 backbuffer 或尝试原地重建。
- 销毁和基础信息查询不受终止状态阻止，调用者仍能诊断并按依赖顺序清理对象。

## F-07C-B 实际结果

- 新增线程安全的内部 `device_status`，首次观察到 DEVICE_LOST 后永久关闭该 Renderer 的 GPU
  操作入口。
- 资源创建与映射同步、Recorder 录制、离屏/窗口提交以及 Fence 等待统一经过门禁和结果观察。
- 资源销毁不受门禁阻止，Device Lost 后仍可按所有权关系释放 CPU 记录并尽力清理 Vulkan 对象。
- 独立测试覆盖正常结果不改变状态、DEVICE_LOST 触发终止以及后续成功结果不能恢复状态。
