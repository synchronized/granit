<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# F-04：Queue 提交与 frames-in-flight

## 元数据

- 设计状态：已确认
- 实现状态：已完成
- 路线图任务：F-04
- 优先级：P0
- 前置依赖：F-01、F-02、F-03、R-08A
- 后续依赖：F-05、F-06、R-08B

## 公共接口

Renderer 描述在末尾追加 `frames_in_flight` 和保留字段。有效范围为 1 到 4，默认值为 2；传入
旧版结构尺寸时不读取新增字段并使用默认值。

```c
granit_command_recorder_end(renderer, recorder);
granit_command_recorder_submit(renderer, recorder);
granit_command_recorder_submit_batch(renderer, recorders, recorder_count);
granit_command_recorder_reset(renderer, recorder);
```

`submit` 只接受 executable Recorder。成功后 Recorder 进入 pending，不能再次 begin、end 或
submit。`reset` 遇到 pending Recorder 时会等待对应 Fence，随后重置 Command Pool 并回到
initial，因此第一版不需要向普通用户公开 Fence。

P-03C 在此模型上增加批量提交：整批先完成句柄、所属 Renderer、重复值和 executable 状态
校验，再按数组顺序通过一次 `vkQueueSubmit2` 提交，共享帧槽、Fence 和提交序号。单 Recorder
接口复用相同内部路径。

## 提交模型

- Renderer 创建固定数量的帧槽，每个槽持有 F-03 的帧上下文。
- 所有 graphics Queue 操作由 Renderer 的 Queue 互斥锁串行化。
- 提交前复用当前槽：若槽仍有提交，先等待其 Fence 并推进完成序号。
- Fence 仅在确定即将调用 `vkQueueSubmit2` 时复位。
- 提交成功后分配连续提交序号、标记 Recorder pending，并轮转到下一槽。
- 提交失败不推进提交序号，并把槽位 Fence 恢复为已触发状态。

F-06 接入交换链后，当前槽的两个二进制 Semaphore 将分别加入 submit 的 wait/signal 列表；本
任务的无窗口提交不使用它们。

## 生命周期

Recorder 录制时保存的资源强引用贯穿 pending 阶段。GPU 完成后 Recorder 回到 executable；引用
在成功 reset 或 destroy 时释放。该策略可能比必要时间多保留资源，但不会提前销毁 GPU 正在使用
的对象。

销毁 pending Recorder 会先等待对应提交。销毁 Renderer 会先使公开句柄失效，再等待所有帧槽，
然后按 Command Recorder、依赖对象和基础资源的顺序释放。

## R-08 接入状态

F-04 已把 `submission_serials` 接入真实 Queue 成功提交和 Fence 完成点。资源暂由 Recorder 强引用
保活，尚未逐资源记录 `last_use_serial` 或转移到 `retirement_queue`；这部分保留为 R-08B，避免在
F-05 尚未形成完整访问记录前建立不完整的资源使用跟踪。

## 线程与错误

- 不同 Recorder 可以并行录制；Queue 提交在内部串行。
- 同一 Recorder 的调用仍需由用户保证不并发。
- 状态错误返回 `GRANIT_ERROR_INVALID_ARGUMENT`，句柄或 Renderer domain 错误返回
  `GRANIT_ERROR_INVALID_HANDLE`。
- Device Lost 会停止正常提交路径；完整恢复边界属于 F-07。

## 验收结果

- 1 到 4 个 frames-in-flight 可配置，旧 Renderer 描述保持兼容。
- 超过帧槽数量的连续提交会安全等待并复用最旧槽位。
- pending Recorder 重复提交会失败，reset 会等待后恢复为 initial。
- Renderer 关闭会等待全部在途提交。
- Clang 动态库和 MSVC 静态库测试覆盖提交状态机与槽位复用。
