<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Buffer

Buffer 是 Renderer 拥有的线性 GPU 资源。C API 使用 64 位整数句柄；C++20 提供 move-only
`granit::buffer` RAII 包装。

## 创建与销毁

`granit_buffer_create` 根据 `granit_buffer_desc` 创建空 Buffer。当前接口不接收初始数据，静态
数据上传将在 R-04 中通过明确的上传路径实现。

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

当前 Buffer 尚不能提交到 GPU 命令中；相关上传、绑定和同步能力由后续路线图任务实现。
