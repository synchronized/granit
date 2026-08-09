<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Buffer

Buffer 是 Renderer 拥有的线性 GPU 资源。C API 使用 64 位整数句柄；C++20 提供 move-only
`granit::buffer` RAII 包装。

## 创建与销毁

`granit_buffer_create` 创建空 Buffer；`granit_buffer_create_with_data` 同步创建并写入覆盖完整
Buffer 的初始数据。`granit_buffer_write` 可同步更新非空局部范围。

UPLOAD 写入直接使用映射内存；DEVICE/AUTOMATIC 写入通过内部 staging 和 Fence 完成。成功返回
后调用者可以立即释放源数据。READBACK 不接受写入。

Buffer 只能由创建它的 Renderer 操作。成功销毁后句柄立即失效；Renderer 销毁也会级联使全部
子 Buffer 失效。重复销毁、错误资源类型和跨 Renderer 操作返回无效句柄。

## 映射

- `UPLOAD` 用于 CPU 写、GPU 读，unmap 自动 flush 写入范围。
- `READBACK` 用于 GPU 写、CPU 读，map 自动 invalidate 读取范围。
- `DEVICE` 和 `AUTOMATIC` 当前不可映射。
- offset 和 size 以字节计，size 不能为零，范围不得越界。
- 返回指针只在 map/unmap 之间有效。
- 同一 Buffer 不支持嵌套映射，也不能在映射期间显式销毁。

内部目前采用持久映射以减少 Vulkan 调用，但使用者不能缓存已经 unmap 的指针。

## C++ 示例

```cpp
granit::buffer buffer;
const auto result = buffer.initialize(
    renderer.native_handle(),
    {.size = 4096,
     .usage = granit::buffer_usage::vertex | granit::buffer_usage::transfer_destination,
     .location = granit::memory_location::device});
```

同步写入适合初始化和低频更新，不适合逐帧大量小写入。高吞吐上传将在后续批量上传接口中实现。
Buffer 已支持 Recorder 中的批量 copy 和 fill。Granit 根据 transfer read/write 意图自动录制
`vkCmdPipelineBarrier2`，普通用户不需要提供 Vulkan Stage 或 Access Mask。
