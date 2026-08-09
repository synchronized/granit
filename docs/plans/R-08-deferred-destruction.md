<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# R-08：GPU 资源延迟销毁基础

## 元数据

- 设计状态：已确认
- 实现状态：已完成
- 路线图任务：R-08
- 优先级：P0
- 前置依赖：R-03、R-05、R-06、V-01
- 后续依赖：R-09、F-01、F-03、F-04

## 背景

公开资源销毁当前会立即使句柄失效，并立即释放对应 Vulkan 对象。现阶段唯一的 Queue 提交是
Buffer 同步上传，函数返回前会等待本次 Fence，因此尚不存在跨 API 调用持续执行的公开 GPU
工作。F-03/F-04 引入异步帧提交后，立即释放仍被已提交 Command Buffer 引用的资源将违反
Vulkan 生命周期规则。

R-08 先建立与后端无关的提交序号和退役队列基础。真正的 GPU 完成值由后续帧同步接入，不能
用“延迟固定帧数”推测资源已经安全。

## 目标

- 公开销毁立即使用户句柄失效，物理资源可以延迟释放。
- 每个已提交 GPU 批次获得单调递增的提交序号。
- 资源记录其最后一次可能被 GPU 使用的提交序号。
- 只有完成序号达到资源退役序号后，才执行物理销毁。
- Renderer 关闭时停止新提交，等待设备空闲并完整排空退役队列。
- 与 V-01 区分“用户仍拥有的活动资源”和“用户已销毁的正常退役资源”。
- 为 Buffer、Texture、Texture View、Sampler 及后续 Pipeline 等资源提供统一机制。

## 非目标

- 本任务不公开 Fence、Timeline Semaphore 或提交序号。
- 不在 R-08 实现 Command Recorder、完整帧循环或 frames-in-flight。
- 不使用后台回收线程，不抽象通用线程池。
- 不保证释放内存后进程常驻内存立即下降；VMA 和驱动可以继续缓存内存块。
- 不允许用户通过整数句柄查询或控制内部退役对象。

## 核心语义

资源具有两个不同的生命周期终点：

```text
用户调用 destroy
→ 公开句柄立即失效
→ 资源进入 retired 状态
→ completed_serial >= retire_after_serial
→ 销毁 Vulkan 对象并释放 allocation
```

`destroy` 成功只表示用户所有权结束和句柄失效，不承诺底层内存已经在返回前释放。该语义对
C API 与 C++ RAII 包装一致，不需要改变公开 ABI。

## 完成点模型

Renderer 内部维护三个 64 位序号：

- `next_submit_serial`：分配给下一次成功提交的候选序号。
- `last_submitted_serial`：最后一次成功提交的序号。
- `completed_serial`：GPU 已确认完成的最大连续序号。

提交失败不能推进 `last_submitted_serial`。序号只用于当前 Renderer 进程内运行期，不能序列化，
也不能跨 Renderer domain 比较。

F-03/F-04 优先使用 Timeline Semaphore 表示完成值；若目标平台最终需要 Fence 回退，则 Fence
必须与提交序号成对记录。无论使用何种 Vulkan 原语，退役队列只依赖单调的
`completed_serial`，不直接依赖 Vulkan 类型。

## 资源最后使用序号

Command Recorder 在记录时保留其引用资源的内部强引用或稳定记录，提交成功后将这些资源的
`last_use_serial` 更新为本次提交序号。用户可以在录制完成后立即销毁公开句柄，但内部记录必须
存活到提交完成。

仅创建但从未提交使用的资源，其 `last_use_serial` 为零，可以在公开销毁后立即回收。当前同步
Buffer 上传在 API 返回前已经等待 Fence，因此上传使用的 staging 对象和目标资源不需要人为
增加一个未完成序号。

禁止使用以下替代方案：

- 销毁后固定等待 2 或 3 帧。
- 根据 CPU 帧号推测 GPU 完成。
- 每次资源销毁调用 `vkDeviceWaitIdle` 或 `vkQueueWaitIdle`。
- 仅依赖 Vulkan Validation Layer 在错误发生后报告。

固定帧延迟在 GPU 卡顿、窗口最小化、提交暂停或不同 frames-in-flight 配置下都不可靠。

## 退役队列

第一版使用 Renderer 私有、按 `retire_after_serial` 非递减排列的队列。每个条目包含：

- 退役序号。
- 内部资源类型。
- 执行物理销毁所需的后端所有权数据。
- V-01 使用的创建序号和可选调试元数据。

同一提交序号的条目可以形成批次，减少容器和完成值查询成本。收集时只处理队首连续满足
`retire_after_serial <= completed_serial` 的批次。

退役队列不保存公开句柄，句柄在入队前已经从 Handle Table 移除。队列中的资源不得重新公开，
也不得被计入“用户遗漏销毁”。V-01 可以单独统计退役数量，用于发现退出时未排空等内部错误。

## 销毁依赖顺序

同一完成点内按照依赖关系销毁：

1. Texture View 等引用资源。
2. Buffer、Texture 和 Sampler 等基础资源。
3. 后续 Descriptor、Pipeline 等资源按实际依赖补充。

Swapchain Backbuffer View 必须先于旧 Swapchain 销毁。Surface 和 Swapchain 的重建与销毁还受
WSI acquire/present 完成状态约束，F-06 接入帧循环时再把其完成点映射到同一模型；不能仅因为
它们也是公开句柄就提前套用普通 Buffer 的规则。

## 收集时机

不创建专用后台线程。后续在以下安全点进行有界收集：

- Queue 提交前后更新完成值时。
- acquire/present 帧边界。
- 资源创建或销毁路径中队列超过阈值时。
- 用户未来显式调用 Renderer 维护或等待接口时。
- Renderer 关闭时强制完整排空。

普通收集不得等待 GPU，只查询已经完成的值。只有显式等待、资源压力恢复和 Renderer 关闭允许
阻塞。

## Renderer 关闭

关闭流程固定为：

```text
冻结新操作与提交
→ V-01 汇总用户仍拥有的资源
→ 公开句柄全部失效并转为退役对象
→ 等待 Device Idle
→ 将 completed_serial 推进到 last_submitted_serial
→ 按依赖顺序排空退役队列
→ 销毁设备与实例
```

`vkDeviceWaitIdle` 只保留在 Renderer 最终关闭或明确恢复路径中，不能成为普通资源销毁实现。
Device Lost 时停止新提交并尽最大努力释放 CPU 与 Vulkan 对象；不得无限等待永远不会完成的
序号。

## 线程与锁

- Registry 锁负责公开身份、domain 和从 active 到 retired 的原子转换。
- 提交互斥锁负责 Queue 外部同步、提交序号和完成值推进。
- 退役队列使用 Renderer 私有锁，不能调用用户回调。
- 不在 Registry 锁内等待 GPU 或销毁可能进入驱动的 Vulkan 对象。
- 锁顺序固定为 Registry 状态转换后释放，再进入提交或退役队列；不能反向持锁。
- 同一资源的销毁与正在进行的 CPU 映射仍按现有规则互斥。

## 分阶段实施

### R-08A：当前基础

1. 实现不含 Vulkan 类型的提交序号与退役队列核心。
2. 支持按完成序号收集、同序号稳定顺序、完整排空和统计。
3. 明确零序号资源立即可回收。
4. 为队列顺序、边界值、未完成资源和关闭排空增加单元测试。
5. 保持当前同步上传和普通资源实际行为不变，避免制造没有真实完成点的伪延迟。

### R-08B：随 F-03/F-04 接入

1. 创建内部 Timeline Semaphore 或 Fence 序号映射。
2. Command Recorder 记录资源引用，提交后更新 `last_use_serial`。
3. 将 Buffer、Texture、View、Sampler 的物理销毁迁移到退役队列。
4. 在帧边界查询完成值并收集。
5. 增加真实异步提交后立即销毁句柄的 Vulkan Validation 测试。

### R-08C：随 F-06 接入 WSI

1. 把旧 Backbuffer View 和旧 Swapchain 纳入 present 完成点。
2. 覆盖重建、out-of-date、最小化和 Surface Lost 路径。
3. 验证不会提前释放正在 present 的对象。

## 测试矩阵

- 完成序号未达到时不释放退役条目。
- 完成序号达到或越过时按顺序释放。
- 同一完成点的 View 在 Texture 前释放。
- 零序号和从未提交使用的资源可立即收集。
- 大量条目可分批收集，统计始终准确。
- Renderer 关闭排空所有条目，不遗留内部强引用。
- V-01 不把正常退役对象报告为用户遗漏。
- 多 Renderer 的序号、队列和资源 domain 完全隔离。
- 后续真实异步提交场景在 Vulkan Validation Layer 下无提前销毁错误。

## 验收标准

- 公开销毁与物理销毁语义明确分离。
- 回收依据真实 GPU 完成值，而不是 CPU 帧数或固定帧延迟。
- 普通资源销毁不调用 Queue/Device Wait Idle。
- 退役队列核心不包含 Vulkan 公共类型，可独立测试。
- Renderer 关闭能够可靠排空，Device Lost 不发生无限等待。
- F-03/F-04 接入时无需改变 C ABI 或重写队列核心。

## 实现结果

R-08A 已完成：

- `submission_serials` 管理连续成功提交、最后提交值和有上限的完成值。
- `retirement_queue` 按退役序号自动排序，不要求资源按最后使用顺序销毁。
- 同一完成点分为 dependent 和 resource 两组，确保引用对象先释放。
- 队列持有类型擦除的内部强引用，不依赖 Vulkan 类型或公开句柄。
- 支持按完成值收集、零序号立即收集、关闭时完整排空和待处理数量查询。
- 单元测试覆盖提交失败不推进、乱序入队、依赖顺序、零序号及关闭排空。

R-08B 已完成：

- 普通提交和 Swapchain 帧提交成功后返回真实 submission serial。
- Recorder 保留的 Buffer 和 Texture View 会在提交成功时更新 `last_use_serial`；View 的强引用
  同时保证父 Texture 存活。
- Buffer、Texture、Texture View 和 Sampler 的公开销毁立即移除句柄，并按最后使用序号进入
  Renderer 私有退役队列。
- acquire、present、Recorder reset 和资源销毁路径会依据 Fence 已完成序号收集，不使用固定
  帧数推测完成状态。
- Renderer 关闭先等待全部提交、释放 Recorder 强引用并排空退役队列，避免队列与 Renderer
  所有权形成残留环。
- Validation Layer 测试覆盖异步提交后立即销毁 Buffer，再等待并回收的路径。

R-08C 已完成：

- 提交 Fence 只证明 signal semaphore 的提交完成，不被误当成 presentation engine 完成点。
- Swapchain 重建、显式销毁、Surface 级联销毁及 Renderer 关闭会在移除旧 backbuffer View 前
  调用 `vkQueueWaitIdle`，建立可靠的 Queue Present 空闲边界。
- Queue 等待发生在 Registry 全局锁之外；等待结束后重新校验对象身份和活动 Frame，避免阻塞
  其他 Renderer 的 Registry 操作。
- Queue 空闲会把普通提交完成序号推进到最后提交值，并收集同期可回收的普通 GPU 资源。
- Device Lost 路径跳过不可能成功的等待，仍允许销毁对象并回收 CPU 侧状态。
- Vulkan 设备初始化要求 `vkQueueWaitIdle` 函数入口存在，避免首次重建时才发现后端不完整。

第一版选择在低频 WSI 重建和销毁路径阻塞 Queue。若以后启用
`VK_EXT_swapchain_maintenance1` 的 present fence，可在保持公共 API 不变的情况下替换为更细粒度
的异步退役。
