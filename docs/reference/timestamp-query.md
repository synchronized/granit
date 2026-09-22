<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Timestamp Query

Timestamp Query Pool 提供后端无关的 GPU 时间戳采集。它只返回 GPU 时间线上的纳秒值，不替代
CPU 墙钟、帧时间或 Present 等待统计。当前能力必须通过
`renderer_limits::supports_timestamp_queries()` 查询；浏览器 WebGPU 当前不提供该能力。

## 公共入口

- C：`<granit/renderer/timestamp_query.h>`。
- C++20：`<granit/renderer/timestamp_query.hpp>`，使用 move-only 的
  `granit::timestamp_query_pool`；Recorder 通过 `timestamp_query_pool::ref()` 借用查询池。
- 所属 CMake component：Core，目标为 `granit::granit`。

## 记录时间戳

创建池时指定固定的 `query_count`，使用
`GRANIT_TIMESTAMP_QUERY_POOL_DESC_VERSION_1_SIZE` 填写描述结构大小，并让每个查询索引只对应
一次待采集位置。录制期间按以下顺序操作：

1. 使用 `granit_command_recorder_reset_timestamp_queries` 重置待复用的查询范围。
2. 使用 `granit_command_recorder_write_timestamp` 在 `TOP`、`DRAW` 或 `BOTTOM` 阶段写入索引。
3. 提交 Recorder 并等待对应 GPU 工作完成。
4. 使用同步或异步结果接口读取纳秒值。

池、Recorder 和 Renderer 必须属于同一资源域。写入和重置要求 Recorder 正处于录制状态；查询
索引和范围不得越过池容量。已经录制或提交的 Recorder 会保留所引用的 Query Pool，直到真实
GPU 完成点；调用方仍应避免与录制或结果读取并发修改同一池。

## 读取结果

`granit_timestamp_query_pool_get_results` 在结果尚未可用时返回
`GRANIT_ERROR_NOT_READY`，成功时将结果写入调用方提供的纳秒数组。它不会把尚未完成的值伪造
为零，也不改变查询池状态。

需要避免阻塞时，使用 `granit_timestamp_query_pool_get_results_async` 创建异步操作，轮询操作
状态，成功后调用 `granit_timestamp_query_pool_copy_results`。异步操作内部持有结果，复制完成后
仍须由调用方销毁异步句柄。

## 所有权与限制

- Query Pool 属于创建它的 Renderer；销毁后句柄立即失效，Renderer 销毁会级联清理。
- 重复销毁、跨 Renderer 使用、错误资源类型和无效查询范围返回相应结果码。
- 不支持 Timestamp Query 的后端创建或写入返回 `GRANIT_ERROR_UNSUPPORTED`。
- C++ 包装只提供 RAII 和类型转换，不建立独立运行时状态。
