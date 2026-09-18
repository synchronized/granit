<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 02：创建资源并上传数据

本教程在[第一个 Renderer](01-first-renderer.md)的基础上创建 Buffer 和 Texture，并使用
Upload Batch 批量提交初始数据。资源句柄只属于创建它们的 Renderer，应用不能跨 Renderer 混用。

## 1. 创建 Buffer

Buffer 描述表达大小、用途和内存位置；它不携带调用方数据的所有权：

```cpp
granit::buffer vertex_buffer;
const auto result = vertex_buffer.initialize(
    renderer.native_handle(),
    {.size = vertices.size_bytes(),
     .usage = granit::buffer_usage::vertex,
     .location = granit::memory_location::device});
```

顶点或索引数据通常放在 Device Local 资源中，再通过 Upload Batch 写入。小型一次性数据也可以
根据用途选择 Upload 或 Host Visible 位置；具体限制见[Buffer 参考](../reference/buffer.md)。

## 2. 创建 Texture

Texture 的 Usage 必须覆盖后续操作。需要采样和传输目标的纹理应同时声明对应用途：

```cpp
granit::texture texture;
check(texture.initialize(renderer.native_handle(), {
    .format = granit::texture_format::rgba8_unorm,
    .usage = granit::texture_usage::sampled |
             granit::texture_usage::transfer_destination,
    .width = 2,
    .height = 2,
}));
```

创建 View 后，Pipeline 或 Bind Group 使用 View 句柄而不是直接使用 Texture 句柄。格式、Mipmap、
View 范围和支持的 Usage 见[Texture 参考](../reference/texture.md)。

## 3. 批量上传

Upload Batch 复制调用方提供的数据，提交成功后调用方可以复用或释放原始数组：

```cpp
granit::upload_batch uploads;
check(uploads.initialize(renderer.native_handle(),
                         {.max_staged_bytes = 4096, .max_operation_count = 8}));
check(uploads.write_buffer(vertex_buffer.native_handle(), 0,
                           std::as_bytes(std::span{vertices})));
check(uploads.write_texture(texture.native_handle(), pixels,
                            {.bytes_per_row = 2 * 4, .rows_per_image = 2},
                            {.width = 2, .height = 2}));
check(uploads.submit());
```

一个 Batch 可以包含多个 Buffer 和 Texture 写入。提交后应通过 `renderer.process_events()` 推进
异步回收；如果需要显式完成状态，使用 `submit_async()` 和 `async_operation`，不要在应用层等待
后端句柄。

## 4. 生命周期顺序

资源由 RAII 包装管理，但提交中的资源仍由 Renderer 保持有效。应用应保持父 Renderer 存活，且
不要在完成前重复使用同一批次的暂存容量：

```text
Renderer
  ├─ Buffer / Texture
  └─ Upload Batch -> submit -> process_events
```

资源类型、所有权、线程规则和提交边界见[Upload Batch 参考](../reference/upload-batch.md)与
[线程安全约定](../reference/thread-safety.md)。

## 5. 与后续 Pipeline 的关系

本教程创建的 Buffer 可以作为 Vertex Buffer，Texture View 和 Sampler 可以进入 Bind Group；
下一篇 Shader/Pipeline 教程使用无绑定的 `SV_VertexID` 三角形来减少首次 Pipeline 的干扰。
当需要真实顶点数据时，再把 Buffer 绑定和 Vertex Layout 加入同一个 Pipeline。
