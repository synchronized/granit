<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.13 迁移到 0.14

0.14.0 为 Upload Batch 增加容量背压与异步完成语义，并让参考 Render Pipeline 内部使用异步
Timestamp。Shader、Material 和 Environment 持久化格式没有变化。

## 必须操作

1. 重新编译应用及使用 Granit C/C++ 头文件的模块。
2. 将 `find_package(granit 0.13 ...)` 更新为 `find_package(granit 0.14 ...)`。
3. 如需非阻塞上传，使用 `granit_upload_batch_submit_async`，轮询返回的异步操作，并在不再查询时
   销毁操作句柄。

## 可选背压

`granit_upload_batch_desc` 可设置 `max_staged_bytes` 和 `max_operation_count`。零表示不设置对应上限。
单次写入自身超过上限返回 `GRANIT_ERROR_INVALID_ARGUMENT`；累计占用将超过上限返回
`GRANIT_ERROR_NOT_READY`。可用 `granit_upload_batch_get_info` 查询当前暂存量，并在稍后重试。

异步提交成功后 Batch 立即清空并可复用。操作成功表示后端不再需要 Batch 的暂存数据；请求取消
不保证撤销已经提交给 GPU 的工作。即使提前销毁异步操作或目标资源，Renderer 也会保留内部依赖
直至后端安全完成。

## 无需操作

- `granit_render_pipeline_get_metrics` 的调用方式与结构布局没有变化。
- 同步 `granit_upload_batch_submit` 继续可用，并保持等待上传完成后返回的语义。
- 无需重新生成 `.grshader`、`.grmat` 或 `.grenv` 资产。
