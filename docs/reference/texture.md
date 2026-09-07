<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Texture 与 Texture View

Texture 拥有图像存储，Texture View 描述如何访问其子资源。两者都是属于 Renderer 的独立 64 位
句柄。

当前支持单采样 2D Texture 和六面 Cube Texture，以及不超过完整链的多个 mip。2D 颜色或深度附件
还支持后端提供的多采样；多采样 Texture 必须只有一个 mip、一个数组层，且不能走 CPU 写入、读取、
复制或 Mipmap 生成路径。Cube Array、普通 2D Array、1D、3D 和格式重解释仍返回
`GRANIT_ERROR_UNSUPPORTED`。

`granit_texture_create_with_default_view` 可以原子创建 Texture 和完整范围 View。默认 View 继承
Texture 格式，并根据颜色、深度或深度模板格式自动选择 aspect。

销毁 View 不影响父 Texture；销毁 Texture 会使其全部 View 句柄立即失效。

## 格式 Footprint

`granit_texture_format_get_footprint` 返回格式块宽高和每块字节数。结果描述 CPU 内存中的紧密排列，
不包含 Vulkan 或具体设备要求的 Buffer Offset、Row Pitch 对齐。BC、ETC2 和 ASTC 格式返回实际
压缩块信息。`granit_texture_format_calculate_data_footprint` 可进一步计算任意尺寸、图片数量的紧密
块列数、块行数、行跨度和总容量，包括尺寸不是块宽高整数倍的末端 mip。

创建前应使用 `granit_renderer_get_texture_format_capabilities` 查询当前设备对指定格式支持的用途、
采样数和线性过滤能力。压缩格式支持随设备与 WebGPU feature 变化，不能仅依据运行平台推断。

## CPU 数据写入

`granit_texture_write` 使用“源数据布局 + 目标区域”描述一次写入：

- `offset` 是像素数据在传入字节区间内的起点。
- `bytes_per_row` 为 0 时使用紧密块行；否则必须不小于紧密行且能被每块字节数整除。
- `rows_per_image` 以块行为单位，为 0 时使用区域块行数；非零值用于表达切片或数组层跨度。
- 目标区域显式选择 mip、数组层、aspect、三维偏移和范围，不能越过对应子资源。
- 压缩区域的起点必须按块对齐；宽高也必须按块对齐，但允许覆盖 mip 右边缘或下边缘的末端块。

函数返回后不再访问调用方的 CPU 数据，当前 Vulkan 后端通过内部 staging buffer 完成同步上传。
Texture 必须带有 `TRANSFER_DESTINATION` 用途。写入支持非压缩和设备支持的压缩颜色格式、单个
mip 与 Cube 面；深度模板写入仍不支持。高频批量上传使用
[Upload Batch](../guides/upload-batch.md)，与同步、异步和 Command Recorder 路径共享相同块布局。

不同 Texture 可以由不同线程同时写入；Queue 提交和全局图像状态由 Renderer 内部排序。同一
Texture 的多个写入、销毁或其他写操作必须由调用方提供顺序。

## 同步原始像素读取

`granit_texture_read` 读取带 `TRANSFER_SOURCE` 用途的非压缩单采样颜色 Texture。第一次以
`data=NULL`、`data_size=0` 调用可获得格式、尺寸、紧密行跨度和所需容量；第二次由调用方提供内存。
容量不足时函数返回 `GRANIT_ERROR_INVALID_ARGUMENT`，更新所需容量且不写入部分数据。

实际读取会内部录制 Texture-to-Buffer、提交并等待 GPU，适合截图、测试和低频工具操作，不适合
每帧视频采集。结果不翻转 Y、不转换颜色空间、不交换 RGBA/BGRA 通道，也不进行图片编码。高级
异步路径继续使用 Command Recorder 与可复用 Readback Buffer。

压缩纹理读回暂不属于公共契约，查询时返回 `GRANIT_ERROR_UNSUPPORTED`。

## Mipmap 生成

`granit_command_recorder_generate_mipmaps` 使用线性 Blit 从指定起始 mip 逐级生成后续 mip。
Texture 必须预先创建完整 mip 存储、为单采样颜色格式，并同时声明 `TRANSFER_SOURCE` 与
`TRANSFER_DESTINATION`。设备格式必须支持 Blit Source、Blit Destination 和线性过滤，否则返回
`GRANIT_ERROR_UNSUPPORTED`。

生成范围可选择起始 mip、级数和 Cube 数组层；级数包含作为源的起始 mip 且至少为 2。接口支持
非二次幂尺寸。运行时生成适合普通颜色纹理；法线重归一化、Alpha Coverage、Gamma 处理和离线
高质量滤波不属于该命令，应由资产管线处理。

压缩纹理不支持运行时 Mipmap 生成；其完整 mip 链应由离线资产管线生成并逐级上传。

同一逻辑纹理需要携带多个设备格式变体或逐 mip 流送时，使用
[Texture Asset Manifest](texture-asset.md)。该契约只负责检查、选择与上传编排，不解析图片容器。
