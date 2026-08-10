<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# P-02：CPU 并发与资源管理性能基线

## 元数据

- 设计状态：已确认
- 实现状态：已完成
- 路线图任务：P-02
- 优先级：P1
- 前置依赖：P-01
- 后续依赖：P-03、P-04

## 背景与目标

P-01 已确认公开线程边界，并验证独立资源上传与 Graphics/Compute 并行录制。P-02 在任何锁粒度、
缓存、批量接口或上传分配器优化之前建立可重复的 CPU 基线，回答以下问题：

- 句柄插入、查询、generation 校验、删除和槽位复用的固定成本及增长趋势。
- Registry 全局锁和资源级锁在多线程资源操作中的等待与扩展效率。
- Command Recorder 创建、录制和资源查找路径的 CPU 成本。
- Queue 锁在并发提交时的吞吐、平均延迟和尾延迟。
- 延迟销毁队列插入、完成点推进和批量回收的成本。
- 每次上传创建 staging buffer 的过渡实现，为 P-04 提供优化前对照。

本任务只建立测量基础设施和基线，不根据单次结果提前重构实现，也不评价 GPU Shader、光栅化或
显存带宽性能。

## 两层测量机制

### 第一层：仓库内置基准

- 使用 `std::chrono::steady_clock`，不增加第三方依赖。
- 基准位于 `benchmarks/`，仅在 `GRANIT_BUILD_BENCHMARKS=ON` 时构建。
- 默认构建、安装导出和普通 `ctest` 不包含基准目标。
- 使用固定预热、重复采样和批量循环，避免计时器开销主导单次操作。
- Debug 只验证功能；正式基线以 Release 构建为准，并同时记录编译器和共享/静态链接模式。
- CPU 微基准不得依赖 Vulkan；Renderer、提交和上传基准单独标记为需要 Vulkan 环境。

第一版输出 CSV 到标准输出，诊断文字写入标准错误，便于脚本保存和比较。每个测试用例输出一行：

```text
schema,name,threads,iterations,samples,total_ns,ns_per_op,p50_ns,p95_ns,p99_ns,ops_per_second
1,handle_lookup,8,1000000,30,...,...,...,...,...,...
```

通用 CPU 用例的 P50/P95/P99 来自各样本归一化后的 `ns/op` 分布；P-02D 需要观察单次提交尾
延迟，因此 Queue 提交用例单独记录每次 `submit` 调用耗时，并用该调用级分布计算三个分位数。
`ns/op` 和 `ops_per_second` 仍按所有并发线程的墙钟时间计算。

命令行至少支持选择用例、线程数、迭代次数、采样次数和预热次数。环境元数据通过 CSV 注释行输出，
包括 Granit 提交、操作系统、CPU、GPU/驱动（适用时）、编译器、构建类型和链接模式。基准失败时
返回非零结果，不输出看似有效的缺失数据。

### 第二层：按需分析工具

P-02 不引入 Profiler SDK，也暂不增加通用埋点宏。第一层发现无法解释的热点后，按问题选择：

- Tracy：用户态时间线、线程区间和锁竞争的首选候选。
- ETW/WPA：Windows 调度、采样和系统级等待分析。
- Visual Studio Profiler：Windows 本地 CPU 采样与调用树确认。
- RenderDoc 或 GPU 厂商工具：只用于区分 CPU 提交等待与 GPU 执行瓶颈，不作为本阶段依赖。

只有确认需要长期、跨平台保留的观测区间后，才评估内部空实现宏和 Tracy 可选后端；任何 Profiler
依赖都不得传播到 Granit 使用者或进入公共 ABI。

已检查 `caors-core` 的 benchmark 实现：其独立目录和 CMake 开关适合复用，但核心依赖 CTrack，
定位包含运行期追踪与通用统计。Granit 当前只借鉴工程组织、结果归档和可选构建方式，不复制业务
用例，也不引入 `ctrack::ctrack`；后续仅在轻量 runner 无法满足已确认需求时重新评估。

## 基准用例

| 子任务 | 场景 | 参数 | 主要指标 |
| --- | --- | --- | --- |
| P-02A | 句柄插入、查询、错误类型/domain、删除与槽位复用 | 表大小、命中率、线程数 | ns/op、ops/s、增长趋势 |
| P-02B | Registry 查询、独立资源创建/销毁与同资源锁竞争 | 1/2/4/8/16 线程 | 扩展效率、P95/P99 |
| P-02C | Recorder 创建、空录制、Buffer 命令和混合工作负载录制 | 命令数、Recorder 数、线程数 | ns/command、ns/Recorder |
| P-02D | 独立 Recorder 并发提交 | 线程数、frames-in-flight | submit 延迟、吞吐、尾延迟 |
| P-02E | 延迟销毁插入、完成点推进和批量回收 | 资源数、批量大小 | ns/resource、回收峰值 |
| P-02F | Buffer/Texture staging 上传 | 数据大小、资源数、线程数 | CPU 耗时、吞吐、分配次数 |

通用线程档位为 1、2、4、8、16，但不得超过测试机逻辑处理器数量后仍将结果解释为扩展能力。
资源规模至少覆盖 100、1,000、10,000；高频命令场景至少覆盖每个 Recorder 1、10、100 条命令。

## 采样与统计规则

- 每个用例先执行不计入结果的预热，默认至少 5 次。
- 正式采样默认至少 30 次；单个样本通过批量迭代达到可稳定计时的持续时间。
- 报告平均值、P50、P95、P99、吞吐量和样本数，不只报告最快一次。
- 多线程用例通过 barrier 同时开始工作；计时范围不包含线程创建和结果格式化，除非用例专门测量它们。
- 同一比较必须使用相同机器、电源策略、后台负载、构建选项和参数。
- Vulkan 用例应先完成设备预热，并将首次 Pipeline 编译等一次性成本拆成独立用例。
- 保存原始输出；汇总工具不能覆盖原始样本或隐藏跳过、失败和环境不可用状态。

初始阶段不设置脱离硬件的绝对性能门槛。以下情况标记为需要调查，而不是自动认定实现错误：

- 相同环境重复运行的中位数波动超过 10%～15%。
- 增加线程后吞吐持续下降或尾延迟异常增长。
- 表大小或资源数量增长时出现非预期的线性、平方级恶化。
- 同环境、同参数下相对已保存基线退化超过 10%。

## 分步实施

1. P-02A / 已完成：建立 `benchmarks/`、CMake 开关、CLI、CSV 输出和纯 CPU 句柄表基准；覆盖
   命中、错误类型/domain、旧 generation 和槽位复用。
2. P-02B / 已完成：接入 Registry 无效句柄查询、独立 upload Buffer 创建/销毁与独立 Buffer
   写入场景；不执行违反公开线程契约的同 Buffer 无序并发写入。
3. P-02C1 / 已完成：测量独立 Recorder 创建/销毁、空 begin/end/reset 周期，以及可配置数量的
   Fill Buffer 命令录制；不包含 Queue 提交。
4. P-02C2 / 已完成：测量共享只读 Pipeline 下的 Graphics/Compute 混合录制路径；每线程独占
   Recorder、颜色附件、Storage Buffer 和 Bind Group，每次循环交替记录一次 draw 与 dispatch，
   不包含 Queue 提交。
5. P-02D / 已完成：在计时前为每个线程准备独立的 executable Recorder 批次，只测量并发
   submit；通过独立参数控制每样本提交数和 1～4 个 frames-in-flight，Recorder 创建、录制、
   pending 等待与销毁均不计入提交区间。
6. P-02E / 已完成：在纯 CPU benchmark 中分别测量预先准备强引用后的退役入队、提交与完成
   序号推进、按完成序号批量收集；通过批量大小控制同一完成点的资源数量，不包含 Renderer
   外层互斥和具体 Vulkan 资源析构成本。
7. P-02F / 已完成：测量 DEVICE Buffer 与 RGBA8 Texture 当前同步 staging 上传路径，覆盖
   4 KiB、64 KiB、1 MiB 和 1/2/4 线程，为 P-04 保存优化前对照。
8. 已完成结果汇总：Registry 和 Queue 并发路径表现出扩展上限与尾延迟；小型 staging 上传由
   每次临时资源和同步提交的固定成本主导；退役队列在共享完成序号批量达到约 100 个资源后
   收集成本趋稳。这些假设交由 P-03/P-04 结合 profiler 继续验证。

## 首份基线索引

- [句柄表](../../benchmarks/results/2026-08-10-windows-clang-handle-table-384aa4e.md)
- [Registry 与资源锁](../../benchmarks/results/2026-08-10-windows-clang-registry-locking-7b85a85.md)
- [基础命令记录](../../benchmarks/results/2026-08-10-windows-clang-command-recording-878264b.md)
- [Graphics/Compute 混合记录](../../benchmarks/results/2026-08-10-windows-clang-mixed-pipeline-recording-30877dc.md)
- [Queue 提交](../../benchmarks/results/2026-08-10-windows-clang-queue-submission-f222bb8.md)
- [延迟销毁队列](../../benchmarks/results/2026-08-10-windows-clang-retirement-queue-2c12645.md)
- [Staging 上传](../../benchmarks/results/2026-08-10-windows-clang-staging-upload-466a588.md)

## 验收标准

- 基准为显式可选目标，不改变普通构建、测试、安装和下游依赖。
- 纯 CPU 用例在无 Vulkan 环境可运行；GPU 用例能明确报告环境不可用。
- 输出结构化、带 schema 版本和完整环境信息，可保存并进行同条件比较。
- 三次相同环境运行能够说明波动范围，结果不依赖单次最优值。
- P-02A 至 P-02F 均保存首份基线，并记录需要 P-03/P-04 验证的具体假设。

## 风险与后续决策

- 微基准可能放大真实帧中不重要的成本，必须结合混合工作负载解释。
- Debug、Validation Layer 和首次驱动编译数据不得作为 Release 常态性能结论。
- `std::chrono` 无法直接给出锁等待原因；只有第一层结果指向明确热点后才使用第二层工具。
- CI 共享机器波动较大，初期只验证基准可运行，不把性能数值作为强制门禁。
