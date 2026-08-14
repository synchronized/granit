<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Upload Batch

Upload Batch 将多次 Buffer 和 Texture 写入合并为一次 Vulkan Queue 提交和一次同步等待。它适合
初始化大量 GPU 资源，或在一帧中集中上传多个小数据块。

当前实现是 P-04B 的同步批量阶段。API 和 ABI 仍处于早期开发阶段，不保证兼容性。

## 基本语义

- `granit_upload_batch_write_buffer` 在返回前复制源数据，之后调用者可以立即修改或释放源内存。
- Buffer 必须属于同一 Renderer，使用 DEVICE 或 AUTOMATIC 内存，并且偏移和大小为 4 的倍数。
- Texture 必须属于同一 Renderer，包含 transfer-destination usage，且写入布局和区域规则与
  `granit_texture_write` 相同。
- 每次写入立即校验。失败的写入不会加入 Batch，也不影响之前已经记录的写入。
- `granit_upload_batch_submit` 同步提交全部写入；成功返回时 GPU 复制已经完成，Batch 已清空并可
  立即复用。
- 空 Batch 提交返回 `GRANIT_ERROR_INVALID_ARGUMENT`。
- `granit_upload_batch_reset` 丢弃尚未提交的写入，不进行 GPU 提交。
- Batch 会保活已记录的目标 Buffer 和 Texture。记录后销毁公开句柄不会使尚未提交的上传悬空。
- 同一个 Batch 的调用应由使用者串行发起；不同 Batch 可以由不同线程独立填充和提交。

## C 示例

```c
granit_upload_batch_desc desc = GRANIT_UPLOAD_BATCH_DESC_INIT;
granit_upload_batch batch = GRANIT_NULL_HANDLE;
granit_result result = granit_upload_batch_create(renderer, &desc, &batch);
if (result == GRANIT_SUCCESS) {
  result = granit_upload_batch_write_buffer(renderer, batch, buffer, 0, data, data_size);
}
if (result == GRANIT_SUCCESS) {
  result = granit_upload_batch_submit(renderer, batch);
}
granit_upload_batch_destroy(renderer, batch);
```

C++20 用户可以使用 move-only 的 `granit::upload_batch`。其析构函数会销毁 Batch，但不会隐式
提交尚未提交的内容。
