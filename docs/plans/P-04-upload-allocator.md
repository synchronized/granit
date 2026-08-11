<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# P-04：持久化上传分配器与批量上传

## 元数据

- 设计状态：已确认
- 实现状态：进行中（进入 P-04A）
- 路线图任务：P-04
- 优先级：P1
- 前置依赖：P-02、P-03、R-04、R-06、R-08
- 后续依赖：P-05、P-06

## 背景与目标

当前 DEVICE/AUTOMATIC Buffer 和 Texture 写入在每次调用中创建并销毁 staging Buffer、Command
Pool、Command Buffer 和 Fence，然后同步等待 GPU。P-02F 表明 4 KiB 与 64 KiB 上传延迟接近，
固定资源和提交成本主导小上传；64 KiB 单次上传约 0.65～0.75 ms，多线程扩展也受单 Queue
串行限制。

P-04 先在不改变现有 API 完成语义的前提下复用上传对象，再为高频逐帧上传提供显式批量边界。
目标是减少 VMA/Vulkan 对象分配、Queue 提交和 Fence 等待次数，同时保持资源状态、Device Lost、
销毁和线程安全语义清晰。

## 语义边界

- 现有 `granit_buffer_write`、`granit_texture_write` 和带数据创建保持同步：成功返回时复制完成，
  调用者可以立即释放源数据。
- 同步 API 可以使用持久化上传对象，但不能把未完成 GPU 工作隐藏到未来帧或 Renderer 销毁。
- 真正异步或批量上传必须使用新的显式 Upload Context/Batch；提交前源数据只保证在写入调用期间
  借用，提交后由内部 staging 内存持有副本。
- 上传顺序必须与提交顺序一致；Texture layout 状态在 Queue 提交顺序确定时推进，不能按 CPU
  完成等待的先后更新。
- Vulkan 类型、Fence、Command Buffer、内存映射和 Queue 仍不得进入公共 API。

## P-04A：可复用同步上传上下文

状态：待实施。

为每个 Renderer 建立内部上传上下文池。每个上下文独占：

- 一个持久映射的 upload staging Buffer；
- 一个 transient/resettable Command Pool 和一个主 Command Buffer；
- 一个 Fence；
- 当前容量、忙碌状态和必要的错误恢复状态。

实施规则：

1. staging 初始按首次请求容量创建，后续不足时按 2 的幂增长；增长只发生在上下文空闲时，旧
   Buffer 在替换前没有未完成提交。
2. 上下文池上限初始采用 Renderer 的 `frames_in_flight`，最少为 1；线程获取独占上下文后可在
   Queue 锁外复制和 flush，避免把大块 CPU memcpy 放进 Queue 临界区。
3. `queue_mutex_` 只保护 Texture 状态解析、`vkQueueSubmit2` 和提交顺序；Fence 等待移到 Queue
   锁外，使独立资源上传可以在同一 Queue 上按序提交后并行等待。
4. Buffer 与 Texture 共用上下文基础设施，但各自记录 Copy 和 Texture barrier；Texture 状态在
   Queue 提交成功后立即按提交顺序更新。
5. Fence 只在即将提交时 reset；提交失败时恢复为 signaled。等待失败会经过统一 Device Lost
   观察路径，上下文在状态不可证明安全时不再复用。
6. Renderer 销毁先等待或确认所有上下文空闲，再按 Command Pool、Fence、staging Buffer 顺序
   释放。内部对象不进入用户泄漏报告。

P-04A 不引入公共 API，也不声称减少每次写入的一次 Queue submit 和一次 Fence wait；它只消除
重复对象创建，并缩短 Queue 锁范围。

## P-04B：上传环与显式批量接口

P-04A 复测证明对象复用有效后，再设计 Upload Context。候选 C API 形态为：

```c
granit_result granit_upload_context_create(
    granit_renderer renderer,
    const granit_upload_context_desc* desc,
    granit_upload_context* context);
granit_result granit_upload_context_write_buffer(...);
granit_result granit_upload_context_write_texture(...);
granit_result granit_upload_context_submit(granit_renderer renderer,
                                           granit_upload_context context);
```

最终接口在实现前还要确认：

- `submit` 是异步返回还是提供显式 wait/reset；普通用户是否需要可查询完成状态。
- 一批内参数错误采用整批失败还是单条写入立即失败；提交失败是否允许重新提交。
- 环形空间按 submission serial 回收，不能按 CPU 帧号或固定 frames-in-flight 推测 GPU 完成。
- Buffer offset、Texture block、`optimalBufferCopyOffsetAlignment` 和 non-coherent atom 对齐统一由
  内部分配器处理。
- 超过环容量的大上传采用独立临时块，不能无上限扩大常驻环；环满时应明确选择等待、扩容或
  返回 `NOT_READY`。
- 同一目标资源的重叠写入、与普通 Recorder 的资源状态衔接，以及销毁后的内部保活必须有测试。

C++20 包装应提供 move-only Upload Context 和 `std::span<const std::byte>`，不建立第二套运行时
状态。批量提交应尽量一次记录、一次 Queue 提交和一个完成序号，不循环调用同步写入 API。

## P-04C：测量与验收

- 使用 P-02F 相同的 4 KiB、64 KiB、1 MiB，1/2/4 线程参数完整重复至少 3 次。
- P-04A 记录同步 Buffer/Texture 写入延迟、吞吐和上下文创建/增长次数。
- P-04B 增加每批 1/10/100 次小上传，分别记录 ns/upload、批次延迟、吞吐和 P95/P99。
- 验证 Validation Layer 无生命周期、同步和 layout 错误。
- 动态与静态库、C11 与 C++20 consumer、Renderer 销毁和 Device Lost 路径均需覆盖。

P-04A 的目标是 4 KiB/64 KiB 同步上传固定成本有可复现下降，并且 1 MiB 路径没有超过 10% 的
稳定回归。P-04B 的目标是批量大小增加时单位上传成本明显下降；若收益不足以抵消公共 API 和
生命周期复杂度，则保留内部复用上下文，不发布批量接口。

## 非目标与停止条件

- 不在 P-04 引入通用线程池、Render Graph、资产流送系统或跨 Renderer 共享 staging 内存。
- 不公开 Vulkan memory type、Command Buffer、Fence 或 timeline semaphore。
- 不为了无锁而允许 staging 区域在 GPU 完成前被覆盖。
- 若驱动 Fence 等待完全主导同步 API，P-04A 在记录结果后停止继续复杂化同步路径，把主要收益
  留给 P-04B 的显式批量边界。
- 若无法可靠处理 Device Lost 或提交失败后的环回收，优先返回错误并停止复用，不猜测 GPU 状态。

