<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Windows Clang Staging 上传基线

## 环境

- Granit：`466a588`
- 构建：Windows Clang 22.1.8、Release、共享库
- 系统：Windows AMD64
- CPU：Intel Core i5-8600K，6 个逻辑处理器
- 每线程每样本上传：10 次
- 重复：每个数据大小和线程档位完整运行 3 次

每次运行包含 2 次预热和 10 个正式样本。Buffer 使用 DEVICE 内存与 transfer-destination usage；
Texture 使用 DEVICE 内存、RGBA8 和完整二维 transfer-destination 区域。目标资源和 CPU 数据在
计时前创建；每次写入均经过当前临时 staging Buffer、临时 Command Pool/Buffer、Fence、Queue
提交和同步等待路径。

## 数据大小扩展

固定单线程。吞吐量由三次平均延迟换算，使用 MiB/s。

| 资源 | 大小 | 平均延迟 | 三次延迟范围 | 吞吐量约值 |
| --- | ---: | ---: | ---: | ---: |
| Buffer | 4 KiB | 0.734 ms | 0.696～0.789 ms | 5.32 MiB/s |
| Buffer | 64 KiB | 0.752 ms | 0.665～0.858 ms | 83.08 MiB/s |
| Buffer | 1 MiB | 1.299 ms | 1.174～1.447 ms | 769.75 MiB/s |
| Texture | 4 KiB | 0.702 ms | 0.616～0.868 ms | 5.56 MiB/s |
| Texture | 64 KiB | 0.649 ms | 0.607～0.705 ms | 96.33 MiB/s |
| Texture | 1 MiB | 0.877 ms | 0.787～0.927 ms | 1,140.31 MiB/s |

## 线程扩展

固定每次上传 64 KiB。吞吐量为全部线程的总吞吐。

| 资源 | 线程 | 平均 ns/op | 三次运行范围 ns/op | 总吞吐量约值 |
| --- | ---: | ---: | ---: | ---: |
| Buffer | 1 | 749,701.667 | 689,232.000～828,937.000 | 83.37 MiB/s |
| Buffer | 2 | 788,014.500 | 738,813.500～876,840.500 | 79.31 MiB/s |
| Buffer | 4 | 587,983.167 | 562,413.500～612,626.000 | 106.30 MiB/s |
| Texture | 1 | 694,791.667 | 535,628.000～856,568.000 | 89.96 MiB/s |
| Texture | 2 | 584,042.000 | 518,631.500～645,226.500 | 107.01 MiB/s |
| Texture | 4 | 539,196.583 | 510,986.250～565,262.750 | 115.91 MiB/s |

## 观察与边界

- 4 KiB 与 64 KiB 的延迟接近，表明临时资源创建、Queue submit 和 Fence 等待等固定成本主导
  小型上传；将许多小上传合并为批次应是 P-04 的首要验证方向。
- 1 MiB 的有效吞吐显著提高，但每次调用仍同步等待 GPU，不能与持久化上传环或异步批量提交的
  带宽直接比较。
- 64 KiB Buffer 从 1 到 2 线程没有吞吐提升，4 线程仅提高约 28%；Texture 到 4 线程提高约
  29%。当前 staging 分配可在 Queue 锁外并行，但实际提交和等待仍受单 graphics Queue 串行路径
  约束，线程扩展收益有限且波动明显。
- Texture 比 Buffer 更快不是通用结论；两条路径包含不同驱动命令与状态跟踪，本样本只用于记录
  当前机器上的优化前对照。
- 基准未直接统计 VMA、Command Pool 或 Fence 分配次数；根据当前实现，每次写入各创建一套临时
  staging 与提交对象。P-04 应补充内部计数或 profiler 证据验证分配热点。
