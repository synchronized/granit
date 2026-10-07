<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Readback Batch

`readback_batch` 把一个或多个 Buffer/Texture 回读请求提交为一个异步操作。它不是
`buffer::map` 的异步版本：`buffer::map` 直接返回已映射 CPU 地址，而 `readback_batch` 先提交
GPU 复制，完成后再把结果复制到调用方缓冲区。

## 生命周期

1. 创建 `readback_batch`，可选设置结果总字节数和请求数量上限；
2. 使用 `read_buffer` 或 `read_texture` 记录请求；失败请求不会加入 Batch；
3. 调用 `submit_async`，成功后 Batch 被清空并可复用，返回的 `async_operation` 持有已提交请求；
4. 轮询 `async_operation::get_status`，只有 `succeeded` 后才能查询结果信息或复制结果；
5. 先用 `get_readback_result_info` 获取所需容量，再调用 `copy_readback_result`；
6. 操作完成、失败或不再需要时销毁 `async_operation`。取消只提出请求，不保证撤销已经提交的 GPU 工作。

Batch、源资源和 Renderer 必须属于同一资源域。提交成功后，调用方可以复用或销毁 Batch；源资源
必须至少保持到 Granit 接收并完成对应 GPU 工作。Renderer 销毁会使未完成操作进入清理路径。

## 结果与容量

- 未完成操作的状态结果为 `GRANIT_ERROR_NOT_READY`，结果信息和结果复制也返回该结果码；
- 终态失败或取消时，结果查询返回操作的最终结果码；
- `copy_readback_result` 的 `data_size` 是输入容量、输出实际所需大小；容量不足返回
  `GRANIT_ERROR_INVALID_ARGUMENT`，同时写回所需大小；
- 传入空指针仅在容量为零且结果大小为零时合法；正常结果应先查询容量并提供足够的目标缓冲区；
- Texture 的 `tight` 布局去除后端行距，`backend` 布局保留后端返回的 `bytes_per_row` 和
  `rows_per_image`。

## 与 `buffer::map` 的区别

`buffer::map` 适用于可映射的 Upload/Readback Buffer，并要求调用方配对 `unmap`；映射期间必须
遵守 Buffer 的同步与写入约束。`readback_batch` 适用于 Device Buffer、Texture 和跨后端异步
回读，结果只通过完成后的复制接口取得，不暴露持久映射地址。
