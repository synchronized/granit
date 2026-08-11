<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Windows xperf 锁竞争归因（7bfa2ee）

## 环境与方法

- 系统：Windows 10 19045，Intel64 Family 6 Model 158，6 个逻辑处理器。
- 构建：Clang 22.1.8、Release、共享库，提交 `7bfa2ee`。
- 符号：额外使用 `-g -gcodeview` 和链接器 `/debug` 生成本地 PDB，不改变仓库配置。
- CPU 跟踪：`PROC_THREAD+LOADER+PROFILE`，采样 `profile` 调用栈。
- 等待跟踪：`PROC_THREAD+LOADER+CSWITCH+DISPATCHER`，采集 `cswitch+readythread`
  调用栈。
- ETW 缓冲区：1024 KiB，最少 128、最多 512；本文采用的跟踪均为 0 丢失事件。
- 原始 ETL 可能包含本机路径和进程名，只保留在本地 `build` 目录，不提交仓库。

下文 CPU 比例以目标 benchmark 进程的独占采样权重归一化。系统 DLL 和 Intel Vulkan 驱动
没有本地私有符号，因而只能归并到模块，不能把其中的每个采样继续拆到函数。

## 结果

### invalid_lookup

参数为每线程 1,000,000 次、30 个样本：

| 线程 | ns/op | `ntdll` | `write_buffer` | `handle_table::find` | MSVC 运行库 |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 37.99 | 32.67% | 27.33% | 10.82% | 10.65% |
| 4 | 49.36 | 84.27% | 5.48% | 2.44% | 4.12% |

4 线程单次成本比 1 线程高约 30%。实现核对表明，无效 Buffer 查询在
`renderer_registry::write_buffer` 的同一 `mutex_` 临界区内依次校验 Renderer 和 Buffer
句柄；`handle_table::find` 不单独加锁。结合 4 线程时 `ntdll` 独占采样从 32.67% 上升到
84.27%，可以把该诊断场景的退化归因于全局 Registry 互斥及其运行库同步路径，而不是
Vulkan 驱动。

该场景刻意查询无效句柄，只能作为竞争上界，不能单独证明应把 Registry 改为
`shared_mutex` 或分片句柄表。

### mixed_pipeline_record

参数为每线程 5,000 次、30 个样本，每次执行 10 组混合命令：

| 线程 | ns/op | `ntdll` | Intel Vulkan 驱动 | `acquire_command_recorder` | `handle_table::find` |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 33,367.34 | 25.51% | 46.13% | 5.73% | 1.52% |
| 4 | 31,554.43 | 72.12% | 13.68% | 2.72% | 0.45% |

4 线程吞吐没有退化，单次成本反而降低约 5.4%。Registry 获取和句柄查询能被采到，但不是
代表性录制路径的首要独占热点；驱动调用与多线程运行库同步占比更高。当前证据不足以承担
`shared_mutex` 的公平性、写者延迟和复杂锁顺序风险，因此 P-03B 不实施 Registry 锁结构改造。

后续可在 Recorder 内部复用已经持有的资源引用，但必须由具体批量接口或录制周期设计驱动，
不能仅为改善无效查询基准缓存裸指针。

### queue_submit

P-02 在相同平台的重复基线已经得到：4 线程相对 2 线程几乎没有吞吐收益，并出现约
201～209 微秒的 P99。代码路径核对确认，每次公开提交会依次获取：

1. Registry 全局锁，用于取得 Recorder 的稳定 `shared_ptr`；
2. Recorder 自身锁，用于保护状态和保留资源；
3. Renderer `queue_mutex_`，并在锁内完成 Fence 槽位复用和 `vkQueueSubmit2`。

这是 Vulkan Queue 外部同步和当前帧槽模型要求的真实串行点。单纯拆掉 `queue_mutex_` 不正确；
缩短 Registry 临界区也不会消除驱动提交和 Fence 复用成本。

尝试在 ETW 堆栈采集期间把提交总量放大到 profiler 所需时长时，采集开销使微基准超过三分钟，
无法得到不受测量工具显著扰动的等待比例。该失败样本不进入数值结论。它不影响 P-02 的非采样
尾延迟证据，但意味着本文不声明 Queue/Fence 的精确等待百分比。

## 决策

- P-03A 完成：热点、1/4 线程差异、锁范围和工具限制均已记录。
- P-03B 停止：代表性混合录制没有证明 Registry 锁结构值得立即复杂化。
- P-03C 继续：设计真正的批量提交，使一次 Queue 锁和一次 Vulkan 提交承载多个 Recorder；
  不能用循环调用现有单提交 API 充当批处理。
- P-04 仍负责 staging 上传环和持久化分配器，不把上传问题并入 P-03。

