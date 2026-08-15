<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Texture 与 Texture View

Texture 拥有图像存储，Texture View 描述如何访问其子资源。两者都是属于 Renderer 的独立 64 位
句柄。

当前支持单采样 2D Texture 和六面 Cube Texture，以及不超过完整链的多个 mip。Cube Array、普通
2D Array、1D、3D、多采样和格式重解释仍返回 `GRANIT_ERROR_UNSUPPORTED`。

`granit_texture_create_with_default_view` 可以原子创建 Texture 和完整范围 View。默认 View 继承
Texture 格式，并根据颜色、深度或深度模板格式自动选择 aspect。

销毁 View 不影响父 Texture；销毁 Texture 会使其全部 View 句柄立即失效。

## 格式 Footprint

`granit_texture_format_get_footprint` 返回格式块宽高和每块字节数。结果描述 CPU 内存中的紧密排列，
不包含 Vulkan 或具体设备要求的 Buffer Offset、Row Pitch 对齐。当前格式均为非压缩格式，因此
块宽高为 `1×1`；保留块语义是为了后续增加压缩格式时不改变查询模型。

## CPU 数据写入

`granit_texture_write` 使用“源数据布局 + 目标区域”描述一次写入：

- `offset` 是像素数据在传入字节区间内的起点。
- `bytes_per_row` 为 0 时使用 `width * 像素字节数`，否则必须不小于紧密行且能被像素大小整除。
- `rows_per_image` 为 0 时使用区域高度；非零值用于表达 3D 切片或数组层之间的跨度。
- 目标区域显式选择 mip、数组层、aspect、三维偏移和范围，不能越过对应子资源。

函数返回后不再访问调用方的 CPU 数据，当前 Vulkan 后端通过内部 staging buffer 完成同步上传。
Texture 必须带有 `TRANSFER_DESTINATION` 用途。首版只支持非压缩颜色格式与单采样 Texture；写入
可以选择单个 mip 和 Cube 面。深度模板写入、压缩格式和 mipmap 自动生成仍属于后续任务，高频
批量上传使用 [Upload Batch](../guides/upload-batch.md)。

不同 Texture 可以由不同线程同时写入；Queue 提交和全局图像状态由 Renderer 内部排序。同一
Texture 的多个写入、销毁或其他写操作必须由调用方提供顺序。

## 同步原始像素读取

`granit_texture_read` 读取带 `TRANSFER_SOURCE` 用途的非压缩单采样颜色 Texture。第一次以
`data=NULL`、`data_size=0` 调用可获得格式、尺寸、紧密行跨度和所需容量；第二次由调用方提供内存。
容量不足时函数返回 `GRANIT_ERROR_INVALID_ARGUMENT`，更新所需容量且不写入部分数据。

实际读取会内部录制 Texture-to-Buffer、提交并等待 GPU，适合截图、测试和低频工具操作，不适合
每帧视频采集。结果不翻转 Y、不转换颜色空间、不交换 RGBA/BGRA 通道，也不进行图片编码。高级
异步路径继续使用 Command Recorder 与可复用 Readback Buffer。
