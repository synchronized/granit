<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Frame 与命令录制分层

Granit 把 GPU 命令录制与窗口帧调度分成两层：Command Recorder 是通用录制原语，Frame Context
管理 acquired Frame 对应的在途槽和 Recorder 复用。离屏任务可以只用前者；实时窗口通常组合两者。
Render Graph 和 Render Pipeline 位于它们之上。

## 分层关系

```text
Render Pipeline（Scene、PBR、阴影、后处理）
        ↓
内部 Render Graph（Pass、依赖、资源状态）
        ↓
Frame Context（窗口 Frame、在途槽、Recorder 复用）
        ↓
Command Recorder（copy、bind、draw、dispatch、submit）
        ↓
Vulkan / WebGPU 后端
```

Frame Context 不是另一套命令 API。`begin` 返回的仍是 Command Recorder，所有复制、绘制和计算命令
都记录到该 Recorder。它增加的是窗口帧所需的槽位选择、复用和失败恢复。

## 对象职责与所有权

| 对象 | 职责 | 所有权与有效期 |
|---|---|---|
| `command_recorder` | 记录和提交 GPU 命令 | 独立创建时由调用方拥有，可用于窗口外任务 |
| `frame_context` | 按 Renderer 的在途槽拥有并复用一组 Recorder | 通常与 Swapchain 帧循环共同存活 |
| `frame_recording` | 表示一次 `begin` 到 `submit/abort` 的录制 | 短生命周期 RAII 对象，不拥有底层 Recorder |
| `acquired_frame` | 关联一次 acquire、提交和 present/cancel | 由 Swapchain 产生，present/cancel 后失效 |
| `swapchain_backbuffer` | 当前 Frame 的输出 Texture/View 引用 | 借用到对应 Frame 结束，不可跨帧缓存 |

`frame_recording::recorder()` 返回借用包装。它只能在 Recording 有效期间使用，不能单独销毁，也不能
保存到下一帧。Frame Context 销毁时负责等待并销毁它所拥有的全部 Recorder。

## 选择哪一层

### 使用 Frame Context

以下工作以 acquired Frame 为提交目标，应优先使用 Frame Context：

- 交换链 Backbuffer 清屏或绘制；
- 每个在途帧槽拥有独立 Uniform、Canvas 或临时上传区域；
- 需要避免每帧立即等待并 reset 同一个 Recorder 的实时窗口；
- 低层自定义渲染流程仍希望复用 Granit 的帧槽同步。

Frame Context 返回的 `frame_slot` 是 Renderer 真实使用的在途槽。它不是 Swapchain 图像索引，也不能
用应用帧序号或句柄取模推导。

### 直接使用 Command Recorder

以下工作不依赖 acquired Frame，可以直接创建 Command Recorder：

- 离屏渲染；
- Buffer/Texture 复制和资源上传；
- Compute 任务；
- 批量提交多个独立 Recorder；
- 调用方自行管理同步和 Recorder reset 的高级执行器。

独立 Recorder 也能通过 `submit(frame)` 提交到 Frame，但实时窗口若还要管理多个在途槽，调用方就
必须自行解决 Recorder 轮转和失败恢复。Frame Context 把这部分固定为统一契约。

### 使用 Render Pipeline

需要 Scene、Material、阴影、IBL、HDR 和后处理时，优先使用 Render Pipeline。调用方仍负责 Window、
Swapchain、acquire 和 present，高层管线负责生成并提交内部命令。不要在同一次 Frame 上同时让
Frame Context 和 Render Pipeline 各自提交一组主渲染命令。

## 窗口帧生命周期

```text
process_events
  → acquire Frame
  → 查询 Backbuffer
  → Frame Context begin
  → 使用借用 Recorder 录制命令
  → Frame Recording submit
  → present Frame
```

`process_events` 不持有 Frame。`begin_rendering`、clear、draw 和 dispatch 是应用选择的命令，不是
Frame Context 固定插入的步骤。只清屏、低层自定义绘制和 Canvas 可以共享同一生命周期。

成功 acquire 后必须走到以下一个终点：

- 正常路径：`begin → submit → present`；
- 录制失败：`abort Recording → cancel Frame`；
- begin 前失败：直接 `cancel Frame`。

`submit` 不执行 present，`abort` 也不取消 Frame。拆开这两个动作可以让 Swapchain 明确完成图像归还、
过期检查和呈现错误处理。

## 为什么需要在途槽

GPU 提交是异步的。应用提交第 N 帧后，CPU 通常会继续准备第 N+1 帧；如果立即 reset 同一个 Recorder
或覆盖同一块动态数据，就必须等待 GPU，失去多帧并行。

Frame Context 按 Renderer 的 `frames_in_flight` 创建槽位。只有当 acquire 再次选中某槽时，它才等待
该槽之前的 GPU 工作完成并 reset 对应 Recorder。与该 Recorder 同寿命的动态资源也应使用
`frame_slot` 选择自己的区域。

## 错误与恢复边界

- `abort` 清除未提交命令并重建该槽 Recorder，适用于录制中途失败。
- `cancel` 结束尚未提交的 acquired Frame，归还 Swapchain 图像。
- `out_of_date` 或 `needs_recreate` 要求在活动 Frame 全部结束后重建 Swapchain。
- `surface_lost` 要求重建 Surface 和 Swapchain。
- `device_lost` 要求停止全部 GPU 路径并重建 Renderer 及资源。

接口、状态和结果码的精确定义见 [Frame Context](../reference/frame-context.md)、
[Command Recorder](../reference/command-recorder.md)和 [Swapchain](../reference/swapchain.md)。
