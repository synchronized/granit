<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# P-04：持久化上传分配器与批量上传

## 元数据

- 设计状态：已确认
- 实现状态：已完成（异步上传环按停止条件暂缓）
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

状态：已完成（`c7f7b63`）。

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

P-04A 已由 `c7f7b63` 完成。Windows Clang Release 复测见
[P-04A 基准](../../benchmarks/results/2026-08-11-windows-clang-upload-context-c7f7b63.md)：
4 KiB/64 KiB 同步上传平均延迟下降 51.5%～70.7%，1 MiB 延迟下降 30.4%～34.0%。

## P-04B：显式同步批量上传

状态：已完成（Buffer/Texture 路径与基准均已完成）。

P-04A 已证明对象复用有效。P-04B 首先提供同步 Upload Batch：写入调用立即把源数据复制到
Batch 持有的 staging 内存，`submit` 将整批命令合并为一次 Queue 提交并等待完成。成功返回后
Batch 自动回到空闲状态，可直接录制下一批。

首版 C API 采用以下形态：

```c
typedef granit_handle granit_upload_batch;

granit_result granit_upload_batch_create(
    granit_renderer renderer,
    const granit_upload_batch_desc* desc,
    granit_upload_batch* batch);
granit_result granit_upload_batch_write_buffer(...);
granit_result granit_upload_batch_write_texture(...);
granit_result granit_upload_batch_submit(granit_renderer renderer,
                                         granit_upload_batch batch);
granit_result granit_upload_batch_reset(granit_renderer renderer,
                                        granit_upload_batch batch);
granit_result granit_upload_batch_destroy(granit_renderer renderer,
                                          granit_upload_batch batch);
```

### 首版语义

- Batch 为 64 位整数句柄，属于创建它的 Renderer；同一 Batch 的方法由对象级互斥保护，但不支持
  多线程同时录制，以免调用顺序变得不确定。不同 Batch 可以并行填充。
- `write_buffer`、`write_texture` 在调用期间借用源数据，并在成功返回前完成 CPU 到 staging 的
  复制。之后调用者可立即释放或修改源数据。
- 每条写入先完整校验参数和目标句柄，再追加到 Batch。单条失败不污染已有条目；不会采用
  “记录错误，等 submit 时整批失败”的延迟错误模型。
- `submit` 对空 Batch 返回 `GRANIT_ERROR_INVALID_ARGUMENT`。提交成功时一次记录全部复制、一次
  Queue submit、一次 Fence wait；成功返回后清空条目并复用内存。
- `submit` 失败后 Batch 进入 failed 状态，调用 `reset` 才能丢弃内容并重新录制。首版不允许直接
  重试，因为部分 Vulkan 命令和资源状态可能已经提交，不能假设失败是原子的。
- `reset` 只丢弃尚未提交的条目和 staging 游标，不释放已经扩大的常驻容量。
- `destroy` 不隐式提交；尚未提交的内容直接丢弃。由于首版 submit 同步等待，销毁时不存在
  Batch 自身的在途 GPU 工作。
- Batch 强引用所有目标资源直到 submit 或 reset 完成，因此用户可以在录制后先销毁公开句柄；
  实际 Vulkan 资源仍保持有效。成功提交后同步完成，再释放这些引用。
- 同一 Batch 中对相同资源的写入严格保持调用顺序。与其他 Batch、普通 Recorder 或同步写入的
  全局先后关系由进入 Renderer Queue 临界区的提交顺序决定。

### staging 分配

- 当前 Batch 以独立 payload 块持有每条写入的 CPU 副本，提交时合并复制到 Renderer 的持久映射
  staging 上下文。若 profiler 证明 CPU 记录分配成为热点，可在不改变公共 API 的前提下改成
  Batch 线性 CPU arena。
- Buffer 数据按 Vulkan copy offset 要求对齐；Texture 数据同时满足格式块、
  `optimalBufferCopyOffsetAlignment` 和 non-coherent atom 相关约束。
- 默认初始容量由内部策略决定，不在首版公共描述结构中暴露调优旋钮。超过常驻阈值的大上传
  使用独立临时块，提交完成后释放，避免一次异常上传永久抬高 Batch 常驻内存。
- 由于一个 Batch 同时最多只有一轮同步提交，首版不需要按 submission serial 回收环形区段，
  也不会覆盖 GPU 尚未读取的数据。

### 暂缓的异步层

首版不提供异步 `submit`、`poll`、`wait` 或 completion token。原因是异步接口还必须同时解决：

- 环形空间按 submission serial 回收，不能按 CPU 帧号或固定 frames-in-flight 推测 GPU 完成。
- completion token 的所有权、查询、等待和 Renderer 销毁行为。
- Batch 销毁后 staging、目标资源和提交对象的延迟保活。
- 环满时选择等待、扩容还是返回 `GRANIT_ERROR_NOT_READY`。
- 与普通 Recorder 之间显式表达依赖，而不是依赖 Queue 偶然的调用顺序。

只有同步批量基准证明 Fence wait 仍是主要瓶颈，并且存在明确的资产流送或逐帧异步需求时，才进入
P-04B2。届时单独设计 Async Upload Context 和 completion token，不改变同步 Batch 的完成语义。

C++20 包装提供 move-only `upload_batch` 和 `std::span<const std::byte>`，不建立第二套运行时状态。
批量提交必须直接走内部批量路径，不能循环调用现有同步写入 API。

## P-04C：测量与验收

- 使用 P-02F 相同的 4 KiB、64 KiB、1 MiB，1/2/4 线程参数完整重复至少 3 次。
- P-04A 记录同步 Buffer/Texture 写入延迟、吞吐和上下文创建/增长次数。
- P-04B 增加每批 1/10/100 次小上传，分别记录 ns/upload、批次延迟、吞吐和 P95/P99，并与逐条
  同步写入对照。
- 验证 Validation Layer 无生命周期、同步和 layout 错误。
- 动态与静态库、C11 与 C++20 consumer、Renderer 销毁和 Device Lost 路径均需覆盖。

P-04A 的目标是 4 KiB/64 KiB 同步上传固定成本有可复现下降，并且 1 MiB 路径没有超过 10% 的
稳定回归。P-04B 的目标是批量大小增加时单位上传成本明显下降；若收益不足以抵消公共 API 和
生命周期复杂度，则保留内部复用上下文，不发布批量接口。

P-04B 基准见
[Upload Batch 基准](../../benchmarks/results/2026-08-11-windows-clang-upload-batch-0ab84b5.md)。
每批 10 次时单位成本下降 86.6%～87.6%，每批 100 次时下降 96.4%～96.7%；单条 Batch 有
8.0%～19.3% 额外开销。因此保留同步单条 API 和显式 Batch 两条路径，并按停止条件暂缓异步环。

## 非目标与停止条件

- 不在 P-04 引入通用线程池、Render Graph、资产流送系统或跨 Renderer 共享 staging 内存。
- 不公开 Vulkan memory type、Command Buffer、Fence 或 timeline semaphore。
- 不为了无锁而允许 staging 区域在 GPU 完成前被覆盖。
- 若驱动 Fence 等待完全主导同步 API，P-04A 在记录结果后停止继续复杂化同步路径，把主要收益
  留给 P-04B 的显式批量边界。
- 若无法可靠处理 Device Lost 或提交失败后的环回收，优先返回错误并停止复用，不猜测 GPU 状态。
