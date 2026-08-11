<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# P-05：线程池与外部执行器边界

## 元数据

- 设计状态：已确认
- 实现状态：已完成（当前结论为不引入线程池、不发布执行器 API）
- 路线图任务：P-05
- 优先级：P2
- 前置依赖：P-01～P-04
- 后续复核点：P-06、H-01

## 决策

Granit 当前不创建内部常驻线程，不要求使用者采用 Granit 的任务系统，也不立即发布外部执行器
接口。现有 Renderer、独立资源和独立 Command Recorder 继续允许由使用者自己的工作线程调用；
Queue 提交由 Granit 内部按提交顺序同步保护。

主要依据：

- 当前 profiler 和 benchmark 没有证明“缺少线程池”是 CPU 瓶颈。
- Vulkan Queue 提交本身需要有序，增加 worker 不能消除这一串行边界。
- 游戏引擎通常已有 Job System；第二套常驻线程会增加过度调度和 CPU 争用风险。
- 动态库内部任务会扩大 Renderer 销毁、Device Lost、DLL 卸载和资源保活语义。
- Shader 编译、文件读取、资源解码和 Scene/Asset 处理不属于当前底层 Renderer 的职责。

## 外部执行器扩展点

P-06 Render Graph 首版仍在调用者线程编译和调度，但内部设计不得依赖特定线程池。真正出现可并行
任务后，可原型验证以下 C ABI 组成：

```c
typedef void (*granit_task_function)(void* task_data);

typedef granit_result (*granit_executor_submit)(
    void* user_data,
    granit_task_function function,
    void* task_data);

typedef granit_result (*granit_executor_wait_group)(
    void* user_data,
    uint64_t group);
```

这只是扩展点轮廓，不是待发布 API。正式设计至少必须明确：

- `user_data`、任务函数和任务数据的所有权与有效期。
- 调度器能否立即在调用线程执行任务，以及是否允许重入 Granit。
- 任务组或 completion token 的创建、完成、等待和错误传播。
- submit 失败后任务是否未执行，以及部分任务已入队时如何收敛。
- Renderer 销毁如何阻止新任务并等待已提交任务完成。
- Device Lost 后未开始任务的取消规则和已运行任务的退出规则。
- DLL 卸载前如何证明没有任务仍持有 Granit 函数地址或内部对象。
- 回调所在线程、可重入性和同步要求必须写入公共文档。

同步执行器应作为第一种原型：`submit` 直接调用函数，`wait_group` 立即成功。它可用于确定性测试，
也能验证 Render Graph 没有暗中依赖 worker 线程。

## 重新评估条件

只有同时具备实际工作负载和 profiler 证据时才重新评估执行器；不能仅因“以后可能并行”提前加入。
建议进入原型验证的条件为：

1. Render Graph 编译或命令录制稳定产生至少 3 个互不依赖、可并行执行的 CPU 任务。
2. 相关单帧 CPU 工作在目标场景中稳定超过 1 ms，而不是偶发尖峰。
3. profiler 证明瓶颈位于可并行 CPU 工作，GPU、Queue wait 和锁竞争不是主要原因。
4. 任务粒度足够大，测得的调度成本不超过任务执行成本的 10%。
5. 外部执行器原型在 4 核及以上 CPU 上使相关帧 CPU 时间稳定下降至少 15%。
6. 至少一个真实使用者缺少 Job System，或外部执行器接入成本已经成为实际采用障碍。
7. Renderer 销毁、Device Lost、取消、错误传播和 DLL 卸载语义均有可测试实现。

满足原型条件也不等于需要内置线程池。优先顺序为：保持调用者线程执行、支持可选外部执行器，
最后才评估可选的 Granit 默认线程池。

## P-06 约束

- Render Graph 数据结构、依赖分析和 Pass 编译不能保存线程本地 Vulkan 状态。
- Pass 录制接口应允许未来把独立 Pass 分配给不同 worker，但首版串行执行。
- 不把 `std::thread`、`std::future`、C++ callable 或线程池对象放进 C ABI。
- 不允许回调跨越 Renderer 或 DLL 生命周期。
- 测试必须能使用同步执行路径稳定复现依赖顺序、错误和资源生命周期。

## 完成条件

P-05 是架构评估任务，不产生代码功能。本文记录决策、扩展点和量化重评门槛后即视为完成；未来
只有 P-06/H-01 的测量满足条件时才重新打开。
