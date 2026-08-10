<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# P-03：锁竞争归因与批量 API 优化

## 元数据

- 设计状态：已确认
- 实现状态：进行中（等待 P-03A 归因）
- 路线图任务：P-03
- 优先级：P1
- 前置依赖：P-02
- 后续依赖：P-04、P-05、P-06

## 背景与目标

P-02 已建立句柄表、Registry、Recorder、Queue、延迟销毁和 staging 上传基线。现有数据说明：

- Registry 无效查询和资源创建在增加线程后出现扩展退化，但样本同时包含句柄表、Registry、
  VMA、驱动和线程调度，尚不能把退化只归因于全局互斥锁。
- Graphics/Compute 混合记录从 2 到 4 线程没有继续提高吞吐，并出现过显著离群样本；需要区分
  Registry 等待、驱动调用和调度抢占。
- Queue 提交从 2 到 4 线程几乎没有吞吐收益，4 线程 P99 达到约 201～209 微秒；单 graphics
  Queue 必然串行，但仍需判断公开调用粒度和锁排队是否放大尾延迟。
- staging 小上传由每次临时资源、提交和同步等待的固定成本主导；该问题交由 P-04 的持久化上传
  分配器处理，不在 P-03 内重复实现一套暂存系统。

本任务先用采样和等待分析确认热点，再实施最小范围的锁或批量接口调整，并用 P-02 相同参数
复测。目标不是消除所有互斥，而是减少无意义的全局串行、动态库调用和细粒度提交开销。

## 非目标

- 不改变 C ABI 的句柄编码、generation、domain 或资源所有权语义。
- 不允许同一 Recorder 或同一可变资源被无序并发操作。
- 不把 Vulkan Queue 包装成虚假的并行执行模型。
- 不在本任务实现上传环、瞬态资源分配器、线程池或 Render Graph。
- 不根据单次微基准结果引入 lock-free 容器或长期维护成本较高的自定义同步原语。

## 分步实施

### P-03A：热点归因

在 Windows Clang Release 共享库上使用 WPR/WPA 或等价 CPU profiler，复测以下场景：

1. `invalid_lookup`：1 与 4 线程，定位 Registry 和句柄表的竞争上界。
2. `mixed_pipeline_record`：1 与 4 线程、每次 10 组，区分 Registry、Recorder 和驱动调用。
3. `queue_submit`：1 与 4 线程、2 个 frames-in-flight，分析 Queue 互斥等待和 Fence 复用。

至少记录 CPU sample 调用栈、上下文切换、线程等待及其调用点。若系统级采集不可用，使用 Visual
Studio CPU Usage 或采样 profiler；不得为了得到结论把长期 profiler SDK 直接加入 Granit。

归因结果必须回答：热点函数、CPU 时间比例、主要等待对象、1/4 线程差异，以及是否足以进入后续
实现。原始 ETL 等大文件保留为本地或 CI 产物，仓库只提交环境、参数和结论摘要。

### P-03B：Registry 读路径

仅当 P-03A 证明 Registry 互斥是代表性录制路径热点时实施。按以下顺序评估：

1. 缩短持锁区间，确保只获取稳定 `shared_ptr` 和校验身份，不在锁内格式化、分配或调用驱动。
2. 将只读查询与结构修改明确分类，评估 `std::shared_mutex`；必须同时验证写者饥饿和创建/销毁
   尾延迟，不能只改善无效查询微基准。
3. 只有单锁仍被证明确认是瓶颈时，才评估按 Renderer domain 或资源类别分片。分片不得破坏跨
   资源原子校验、销毁顺序或固定锁顺序。

不缓存跨公开调用长期有效的裸内部指针；Recorder 在一次录制周期中已经持有的稳定资源引用可在
内部复用，但 reset、destroy 和 Renderer 级联失效后必须释放。

### P-03C：Queue 提交批量化

P-03A 若确认多线程尾延迟主要来自多个细粒度 submit 在 Queue 互斥上排队，则先设计批量 C API：

```c
granit_result granit_command_recorder_submit_batch(
    granit_renderer renderer,
    const granit_command_recorder* recorders,
    uint32_t recorder_count);
```

该草案在实现前仍需确认：全部 Recorder 的原子校验与失败语义、提交顺序、每个 Recorder 的
pending 状态、frames-in-flight 槽位分配、部分 `vkQueueSubmit2` 失败的恢复，以及 C++20 包装的
`std::span` 接口。不能简单循环调用现有公开函数后宣称完成批量优化。

若驱动提交本身完全主导且批量接口不能减少 Queue/Fence 操作，则保留现有 API，只记录结论。

### P-03D：复测与收尾

- 使用 P-02B、P-02C2 和 P-02D 的相同环境及参数复测。
- 至少完整重复 3 次，记录平均值、范围和 Queue 单次提交 P50/P95/P99。
- 比较吞吐、尾延迟和单线程回归；任何优化不得只改善诊断型无效查询而恶化混合工作负载。
- 若增加公共批量 API，同步补充 C11 头文件测试、C 行为测试、C++20 RAII/`span` 测试、共享与
  静态库 consumer、安装导出和文档。

## 验收标准

- 每项锁或 API 改动均有 profiler 证据和同条件前后基准，不以经验推断代替测量。
- 代表性多线程路径吞吐或尾延迟有可复现改善，或者明确记录“不值得修改”的结论。
- 单线程正常路径没有超过 5%～10% 且无法解释的稳定退化。
- 线程安全、句柄失效、资源所有权、Device Lost 和销毁顺序保持不变。
- 不把 profiler、平台工具或 benchmark 依赖传播到 Granit 使用者。

## 风险与停止条件

- ETW 采样本身可能改变短时微基准调度，必须结合无采样基线解释。
- `shared_mutex` 在 Windows 实现上的读写公平性可能不适合高频创建/销毁路径。
- Vulkan Queue 的外部同步要求不会因减少 Granit 锁而消失；错误地并行调用 Queue 属于功能缺陷。
- 若等待主要来自驱动或 GPU Fence，P-03 停止拆锁，把批量/异步上传交给 P-04。
- 若优化收益落在重复波动范围内，应回退复杂化改动并保留测量结论。
