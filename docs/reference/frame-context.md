<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Frame Context

Frame Context 按 Renderer 的真实在途帧槽轮转一组 Command Recorder，避免实时窗口每帧提交后立即
等待并 reset 同一个 Recorder。当前只提供 C ABI；C++20 RAII 包装将在 F-10C 增加。

## 创建与帧循环

```c
granit_frame_context_desc desc = GRANIT_FRAME_CONTEXT_DESC_INIT;
granit_frame_context context = GRANIT_NULL_HANDLE;
granit_frame_context_create(renderer, &desc, &context);

granit_command_recorder recorder = GRANIT_NULL_HANDLE;
uint32_t frame_slot = 0;
granit_frame_context_begin(renderer, context, frame, &recorder, &frame_slot);
/* 使用借用的 recorder 录制命令。 */
granit_frame_context_submit(renderer, context, frame);
granit_swapchain_present(renderer, swapchain, frame, &needs_recreate);
```

`begin` 使用 Frame acquire 时获得的真实槽位。若该槽已有已提交 Recorder，它只在槽位再次使用时
等待 GPU 完成并 reset。返回的 `frame_slot` 可用于选择相同生命周期的上传资源，不能替换为
Swapchain 图像索引或本地帧序号。

`submit` 会结束 Recorder 并通过 Frame 提交，但不会执行 present。成功后该 Frame 不能再次 begin
或 submit。调用方仍负责处理 present、OUT_OF_DATE、Surface Lost 与 Device Lost。

## 放弃与销毁

录制失败且尚未提交时，调用 `granit_frame_context_abort`。它会销毁并重建该槽 Recorder，清除未提交
命令和内部资源引用，但不会取消 Frame；调用方之后仍须调用 `granit_frame_cancel`。

Context 拥有全部 Recorder。`begin` 返回的句柄只能在录制期间借用，对它调用
`granit_command_recorder_destroy` 返回 `GRANIT_ERROR_UNSUPPORTED`。销毁 Context 会先使 Context
句柄失效，再等待已提交 Recorder 并销毁全部槽；Renderer 销毁会级联执行相同资源清理。

## 状态与错误

- 同一槽位的状态为 idle、recording 或 submitted。
- 对 recording 槽重复 begin、用错误 Frame submit/abort，返回
  `GRANIT_ERROR_INVALID_ARGUMENT`。
- Renderer、Context 或 Frame 不属于同一 Renderer，返回 `GRANIT_ERROR_INVALID_HANDLE`。
- 单个 Context 从 begin 到 submit/abort 之间由调用方独占；不同 Context 可独立使用。
- 描述结构的 flags 和 reserved 当前必须为零。

Frame 的获取、呈现和失效规则见 [Swapchain 参考](swapchain.md)。
