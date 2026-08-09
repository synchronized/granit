<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# F-06：Swapchain 帧循环

## 元数据

- 设计状态：已确认
- 实现状态：F-06A/F-06B 已完成
- 路线图任务：F-06
- 优先级：P0
- 前置依赖：F-03、F-04、F-05、R-07
- 后续依赖：F-07、R-08C

## 目标

建立完整且不会泄漏 Vulkan WSI 细节的 acquire → record → submit → present 流程。普通用户不接触
Semaphore、Fence、`VkResult` 或 Image Layout，但必须能够识别需要重建 Swapchain 的状态。

## F-06A：Vulkan 后端原语（已完成）

`vulkan_swapchain` 提供内部 acquire 和 present：

- acquire 使用调用方提供的二进制 Semaphore，无限等待可用图像；
- 校验返回图像索引属于当前 Swapchain；
- present 明确接收 Queue、图像索引和等待 Semaphore；
- `VK_ERROR_OUT_OF_DATE_KHR` 映射为 `GRANIT_ERROR_OUT_OF_DATE`；
- `VK_SUBOPTIMAL_KHR` 作为操作成功并通过内部 `suboptimal` 标志保留，不能误报为失败；
- 设备初始化在启用 Surface 时检查 `vkAcquireNextImageKHR` 和 `vkQueuePresentKHR`。

## F-06B：公共帧令牌与帧槽接入（已完成）

使用短生命周期、不透明的 `granit_frame` 令牌，而不是让 submit 隐式绑定“最近一次 acquire”：

```text
swapchain_acquire → frame + image_index
record commands using backbuffer(image_index)
command_recorder_submit_frame(recorder, frame)
swapchain_present(frame) → frame 失效
```

帧令牌明确关联 Renderer、Swapchain、图像索引和 frames-in-flight 槽位，可避免以下歧义：

- 同一 Renderer 拥有多个 Swapchain；
- acquire 后提交了一个与窗口无关的离屏 Recorder；
- 错误地用 A Swapchain acquire 的图像向 B Swapchain present；
- present、重复提交或重建后继续使用旧帧。

令牌是瞬时状态，不代表可持久化资源，也不暴露 Fence。普通 `command_recorder_submit` 继续用于
不依赖 Swapchain 的离屏提交。

## 帧槽同步顺序

1. acquire 前等待并回收将要复用的帧槽；
2. `vkAcquireNextImageKHR` 触发槽位的 `image_available`；
3. 提交前导 Command Buffer，把 acquired 图像从已知状态转换为 Attachment Layout；
4. Queue submit 等待 `image_available`，执行前导命令和用户 Recorder；
5. 提交后导 Command Buffer，把图像转换为 `PRESENT_SRC_KHR`；
6. Queue signal `render_finished` 和槽位 Fence；
7. present 等待 `render_finished`，随后使帧令牌失效。

Fence 只在确定 Queue submit 即将发生时复位。成功 acquire 的 Frame 最终必须完成 submit 和
present，或者通过 F-07A 的取消路径归还图像；活动 Frame 存在时拒绝重建或销毁对应
Swapchain/Surface。Renderer 关闭仍会等待 GPU 并回收全部瞬时令牌。

## 状态与重建

- acquire 返回 OUT_OF_DATE：不产生帧令牌，调用者重建后重试。
- acquire 返回 SUBOPTIMAL：仍产生可用帧，并记录“建议重建”。
- present 返回 OUT_OF_DATE：帧令牌失效，调用者重建。
- present 返回 SUBOPTIMAL：本帧成功，帧令牌失效，并建议随后重建。
- recreate 前必须处理或取消当前 Swapchain 的所有 acquired frame。
- 重建成功后旧 backbuffer、view 和 frame 句柄全部失效。

公共层通过 acquire/present 的 `needs_recreate` 输出表达 SUBOPTIMAL，不把它当成错误。C++
`acquired_frame` 保存该布尔值。

## 线程与锁

- Registry 锁只验证 Frame、Swapchain、Recorder 的 identity 和 domain。
- Queue 互斥锁覆盖 acquire 所需槽位切换、submit 和 present。
- 单个 Frame 令牌不允许并发操作。
- 窗口事件处理和 Swapchain 重建不能与对应帧操作并发。

## F-06A 验收结果

- 后端 acquire/present 参数和索引边界有明确校验。
- OUT_OF_DATE 与 SUBOPTIMAL 不混为同一种失败。
- Vulkan 函数表缺失时设备初始化失败，而不是延迟到首帧崩溃。
- 两种 Windows 工具链构建并运行后端边界测试。

## F-06B 验收结果

- acquire 返回 Frame 令牌和 backbuffer 索引。
- `submit_frame` 校验 Frame、Recorder、Renderer 和 Swapchain 归属。
- Queue submit 等待 image-available，按前导、用户、后导顺序执行，并触发 render-finished。
- 后导屏障把 acquired 图像转换为 `PRESENT_SRC_KHR`。
- present 消费 Frame 令牌，重复提交、重复 present 和旧令牌均失败。
- 活动 Frame 会阻止 Swapchain 重建、Swapchain 销毁和 Surface 级联销毁。
- Win32 测试在 Vulkan Validation Layer 下完成真实 acquire、clear、submit 和 present。
