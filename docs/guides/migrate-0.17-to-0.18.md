<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.17 迁移到 0.18

0.18.0 增加 GPU 块压缩纹理、紧密布局计算和设备格式能力查询。既有非压缩纹理调用无需改变，
但 0.x Consumer 应重新编译并把 CMake 请求版本更新为 0.18。

## 选择压缩资产变体

先调用 `granit_renderer_get_texture_format_capabilities` 查询候选格式。只有
`supported_usage` 同时包含应用所需用途时才创建 Texture；零能力表示当前设备或 WebGPU Device
未启用该压缩格式族，应选择其他压缩变体或 RGBA 回退。

## 计算与上传块数据

使用 `granit_texture_format_calculate_data_footprint` 计算每个 mip 的块列、块行和紧密容量。
压缩区域的起点必须按块对齐；宽高必须按块对齐或恰好抵达 mip 边缘。`rows_per_image` 对压缩格式
以块行为单位。同步写入、Upload Batch、异步批次和 Command Recorder 使用相同规则。

压缩纹理的完整 mip 链应由离线资产管线生成。0.18.0 不支持压缩纹理读回和运行时 Mipmap 生成。
