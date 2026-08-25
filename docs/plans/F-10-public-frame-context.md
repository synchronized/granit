<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# F-10：公共帧上下文与 Recorder 轮转

## 状态

- 设计状态：已确认
- 实现状态：F-10A～F-10C 已实现，F-10D～F-10F 待开始
- 路线图任务：F-10
- 优先级：P1
- 前置依赖：F-03、F-04、F-06、F-07、S-09A

## 背景与目标

Renderer 内部已经按 `frames_in_flight` 保存 Fence、Semaphore 和提交上下文，Swapchain acquire
也会等待即将复用的真实帧槽。但是公共帧令牌没有暴露稳定的槽位身份，窗口示例通常只创建一个
Command Recorder，并在每次 submit/present 后立即调用 `reset`。`reset` 必须等待该 Recorder 的
GPU 提交完成，因而把本可并行的 CPU 录制与 GPU 执行重新串行化。

SDL3 + ImGui 示例已经用三个 Recorder 和三个 Canvas 上传槽验证了问题与优化方向，但当前实现仍有
以下限制：

- 示例用本地帧序号推导槽位，而不是使用 Swapchain acquire 的真实槽位；
- Canvas 把上传槽数量固定为三个，未与 Renderer 支持的 1～4 个在途帧统一；
- Clear、Triangle、HDR 等窗口路径仍在每帧立即 reset；
- 每个调用方都需要重复实现 Recorder 数组、复用等待、失败清理和退出排空。

本任务目标是建立一个后端无关、可由 C11 表达并由 C++20 轻量包装的公共帧上下文，使实时窗口
渲染默认采用安全的多帧轮转，并让临时上传资源共享同一个真实 `frame_slot`。

## 非目标

- 不实现 Bindless Resource Table；帧槽管理临时数据生命周期，Bindless 管理长期 GPU 资源索引。
- 不公开 Fence、Semaphore、Queue、Command Pool 或任何 Vulkan 类型。
- 不改变需要立即 CPU 回读结果的离屏、Compute、截图和测试路径的同步语义。
- 不把 Render Graph、Material 或 Render Pipeline 内部状态移入核心帧上下文。
- 不通过无限增加 frames-in-flight 掩盖 GPU、Present 或事件循环瓶颈。
- 不在 C++ 包装层建立与 C ABI 平行的第二套运行时状态。

## 现状证据

### 已确认存在串行等待的实时窗口路径

- `window_clear`
- `sdl3_window_clear`
- `xcb_window_clear`
- `wayland_window_clear`
- `window_triangle`
- `window_hdr`

这些路径都采用“单 Recorder → submit → present → 当帧 reset”。SDL3 + ImGui 改为三槽轮转后，
已经证明 Recorder 只能在对应槽位再次使用前 reset；Canvas 上传 Buffer 也必须在同一完成点之后
复用。

### 应保留同步的路径

`texture_readback`、`compute`、`offscreen_clear`、`offscreen_triangle`、`pbr_offscreen` 及相关 GPU
回归测试会在提交后读取结果或只执行一次。它们的 reset 是显式完成点，不属于本任务的自动迁移
范围。后续只有异步回读出现独立需求和基准时才另建计划。

## 已确认决策

### 真实帧槽是唯一事实来源

公共槽位必须来自成功 acquire 的 `granit_frame`，不能由调用方用帧计数、Swapchain 图像索引或句柄
取模推导。图像索引与 frames-in-flight 槽位不是同一概念，Swapchain 重建、OUT_OF_DATE 和取消帧
也可能打断本地计数。

计划新增可扩展的 `granit_frame_info` 查询结构，至少返回：

- `struct_size`；
- `frame_slot`；
- `frame_slot_count`；
- 预留且必须为零的扩展字段。

查询只借用 Frame，不延长其生命周期。Frame 被 present、cancel 或 Renderer 级联销毁后，旧句柄
继续返回无效句柄。

### 公共帧上下文只管理逐帧录制状态

核心 C ABI 计划提供 move-opaque 的 `granit_frame_context` 句柄，内部按 Renderer 的
`frames_in_flight` 创建并拥有对应数量的 Command Recorder。首版职责限定为：

1. 用 Frame 的真实槽位选择 Recorder；
2. 仅在槽位再次使用时等待并 reset；
3. begin 后把 Recorder 借给调用方录制；
4. 统一 end、submit_frame、失败取消和退出排空；
5. 返回同一 `frame_slot`，供上传环和高层组件使用。

接口草案在实现前通过 C consumer 固化，命名暂定为：

```c
granit_frame_context_create(...);
granit_frame_context_begin(..., granit_frame frame, granit_command_recorder* recorder,
                           uint32_t* frame_slot);
granit_frame_context_submit(..., granit_frame frame);
granit_frame_context_abort(..., granit_frame frame);
granit_frame_context_destroy(...);
```

`begin` 到 `submit/abort` 之间，同一个 Frame Context 和返回的 Recorder 由调用方独占。`submit`
只结束并提交 Recorder，不执行 present；Swapchain 仍负责呈现。具体错误码、重复调用和录制中失败
行为必须在 F-10A 原型测试后冻结，不能直接按草案发布。

### C++ 包装保持轻量

C++20 层只包装 C 句柄和借用的 Recorder，提供移动语义、作用域失败清理与明确的 `submit`。包装层
不得自行保存另一组 Recorder 数组，也不得用析构静默吞掉正常提交失败。调用方仍显式处理 acquire、
present、OUT_OF_DATE 和 Swapchain recreate。

### 所有逐帧上传统一按槽位索引

Canvas、动态顶点、Uniform/常量及后续逐帧描述符更新只能接受 Frame Context 返回的槽位。组件必须
支持 Renderer 允许的 1～4 个槽位，不能继续固定为三个；每个槽的资源只在对应 Recorder 完成后
覆盖。长期资源仍由普通句柄和延迟销毁管理，不进入帧上下文。

## 状态与错误语义草案

单个 Frame Context 槽位至少区分 `idle`、`recording`、`submitted`：

- `begin` 只接受有效、未提交且属于同一 Renderer 的 Frame；
- `recording` 状态重复 begin、跨 Frame submit 或跨 Renderer 使用返回明确错误；
- `submit` 成功后 Frame Context 不再允许修改该 Recorder，直到同槽下一次 begin；
- `abort` 只处理尚未成功提交的录制，不能伪造 GPU 完成；
- 销毁 Context 必须等待其所有已提交 Recorder，先使公共句柄失效，再释放内部对象；
- Device Lost、Surface Lost 和 OUT_OF_DATE 继续沿用 F-07，不新增重叠结果码。

若 Vulkan 不允许安全终止某个已 begin 但录制失败的 Command Buffer，内部应销毁并重建该槽的
Recorder，不能提交部分命令，也不能把无效状态留给下一帧。

## 实施顺序

1. **F-10A 契约原型（已完成）**：增加 Frame Info 的内部查询与纯 C/C++ 头测试，验证真实槽位在
   acquire、present、cancel 和重建中的生命周期；用测试冻结无效结构尺寸、跨对象与旧句柄语义。
2. **F-10B 帧上下文 C ABI（已完成）**：实现句柄、状态机、Recorder 所有权、submit/abort/销毁
   和诊断；覆盖 1、2、3、4 个 frames-in-flight。
3. **F-10C C++20 包装（已完成）**：提供 move-only RAII 和非拥有 Recorder 包装，不复制运行时
   状态；正常提交显式返回结果，未提交录制在析构时 abort。
4. **F-10D 上传槽统一**：移除 Canvas 固定三槽假设，使上传容量按 Context 槽数初始化并由真实
   `frame_slot` 选择；旧 V1/V2 描述结构保持兼容或提供明确迁移。
5. **F-10E 示例迁移**：迁移 Clear、Triangle、HDR、SDL3、XCB 和 Wayland 实时窗口示例；
   `sdl3_imgui` 删除本地 Recorder 数组，Render Pipeline Window 审计后复用同一抽象。
6. **F-10F 性能与文档**：记录 CPU 帧时间、GPU 时间、Present 等待和槽位等待；更新 Reference、
   Guide、安装 Consumer 与示例说明，再决定是否将其作为推荐默认帧循环。

## 测试与验收

- 公共 `.h` 可由 C11 独立包含，`.hpp` 可由 C++20 Consumer 使用。
- Frame Info 和 Context 结构均使用 `struct_size`，新增字段只追加在末尾。
- 覆盖无效 Frame、跨 Renderer/Swapchain、重复 begin/submit/abort、旧句柄和销毁中等待。
- 覆盖 1～4 个 frames-in-flight、Swapchain 重建、SUBOPTIMAL、OUT_OF_DATE 和 Device Lost。
- 至少连续运行 2,000 帧，并验证 Recorder、Frame、上传 Buffer 和退役资源不泄漏。
- 迁移后的窗口示例不在每次 present 后立即等待同一 Recorder。
- Canvas 与其他逐帧上传不会覆盖仍由 GPU 使用的槽位，也不依赖 Swapchain image index。
- 同步回读示例行为和测试保持不变。
- Windows/Linux、共享/静态、C/C++ Consumer、安装导出和 ABI 检查全部通过。
- 性能验收同时报告吞吐与输入延迟；若额外在途帧只提高 FPS 却造成不可接受延迟，应提供 1～4
  槽配置而不是固定默认值。

## 风险与未决问题

- `granit_frame_context` 是否应只管理 Recorder，还是同时提供可扩展的逐帧用户数据挂接点；首版
  倾向前者，避免形成通用对象容器。
- `abort` 对 recording Recorder 的跨后端可实现性需要先通过 Vulkan 原型确认。
- Render Pipeline 当前部分入口内部创建、提交并等待 Recorder；窗口入口是否应改为接收外部
  Frame Context，必须避免同时保留两套提交模型。
- Canvas V2 已公开固定三槽语义。若改为动态槽数，需要保持旧调用方安全，并为 4 槽 Renderer
  定义兼容行为。
- 多帧轮转可能增加输入到显示的延迟；默认槽数应跟随 Renderer 配置，不由高层组件擅自增加。
- 公共 C ABI 只在 F-10A 证明 C++ 包装无法单独解决真实槽位和所有权问题后冻结；若内部行为或
  轻量包装即可满足代表性路径，应缩小 ABI，而不是为了接口对称扩大表面。
