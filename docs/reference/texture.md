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
