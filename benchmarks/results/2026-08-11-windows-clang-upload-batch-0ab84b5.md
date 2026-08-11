<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Windows Clang Upload Batch 基准

## 环境与方法

- Granit：`0ab84b5`，功能实现基于 `78753b5`
- 构建：Windows Clang 22.1.8、Release、共享库
- 系统：Windows AMD64
- CPU：Intel Core i5-8600K，6 个逻辑处理器
- 数据：单线程，每次写入 4 KiB
- 每组运行：2 次预热、10 个正式样本，完整重复 3 次

逐条路径每次写入各执行一次 Queue submit 和 Fence wait。Batch 路径将 1、10 或 100 次写入记录到
同一个 Upload Batch，再执行一次 Queue submit 和 Fence wait。`ns/upload` 包含源数据复制、记录、
提交和等待，按 Batch 中的写入数归一化。

## 结果

| 资源 | 每批写入数 | 平均 ns/upload | 三次运行范围 | 平均批次延迟 | 相对逐条路径 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Buffer 逐条 | 1 | 392,783 | 373,840～424,960 | 0.393 ms | 基线 |
| Buffer Batch | 1 | 468,543 | 422,760～498,910 | 0.469 ms | +19.3% |
| Buffer Batch | 10 | 48,518 | 44,178～50,916 | 0.485 ms | -87.6% |
| Buffer Batch | 100 | 12,968 | 12,027～14,069 | 1.297 ms | -96.7% |
| Texture 逐条 | 1 | 394,403 | 354,090～452,860 | 0.394 ms | 基线 |
| Texture Batch | 1 | 425,757 | 372,520～519,870 | 0.426 ms | +8.0% |
| Texture Batch | 10 | 52,988 | 52,633～53,427 | 0.530 ms | -86.6% |
| Texture Batch | 100 | 14,351 | 13,647～14,902 | 1.435 ms | -96.4% |

10 个样本下 P95 与 P99 落在同一个最高样本。三次运行的归一化 P99 平均值如下：

| 资源 | 每批写入数 | 平均 P95/P99 ns/upload |
| --- | ---: | ---: |
| Buffer Batch | 1 | 636,400 |
| Buffer Batch | 10 | 65,297 |
| Buffer Batch | 100 | 15,492 |
| Texture Batch | 1 | 542,133 |
| Texture Batch | 10 | 63,350 |
| Texture Batch | 100 | 16,637 |

## 结论

- Batch 为 1 时比现有单次同步写入慢 8.0%～19.3%，原因是多了一层公开句柄查询、记录容器和
  数据所有权复制。单次上传应继续使用 `granit_buffer_write` 或 `granit_texture_write`。
- Batch 为 10 时单位上传成本下降约 86.6%～87.6%；Batch 为 100 时下降约 96.4%～96.7%，
  证明合并 Queue submit 和 Fence wait 能有效摊薄固定成本。
- 100 次 Batch 的总延迟约 1.30～1.44 ms，说明数据复制和逐条记录成本开始显现，但仍远低于
  100 次同步提交。当前没有数据支持立即增加异步上传环。
- 后续若 profiler 表明 CPU 记录分配成为热点，可把 Batch 当前的独立 payload 块改成线性 CPU
  arena；该优化不需要改变公共 API。
