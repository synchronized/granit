<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Texture Asset Manifest

Texture Asset Manifest 是 Granit 用于检查和选择 GPU 纹理变体的后端无关元数据。它不包含像素
负载，也不是 KTX2、DDS 或通用图片容器。调用方持有 Manifest 与负载内存，Granit 不保存其指针。

## 公共操作

- `granit_texture_asset_encode` 将调用方提供的元数据确定性编码为 v1 Manifest。接口采用两次调用
  模式，先查询所需字节数，再写入调用方缓冲区；相同输入产生完全相同的字节序列。
- `granit_texture_asset_inspect` 严格校验 Manifest。首次将两个输出数组保持为空可查询数量，随后由
  调用方提供足够容量取得变体和子资源摘要。
- `granit_renderer_select_texture_asset_variant` 按 Manifest 顺序返回首个同时满足声明用途、当前设备
  格式能力和调用方 feature 条件的变体；没有兼容变体时返回 `GRANIT_ERROR_UNSUPPORTED`。
- `granit_upload_batch_write_texture_asset_mips` 校验所选变体负载的 SHA-256，并把指定 mip 范围原子
  加入一个空 Upload Batch。Batch 容量不足返回 `GRANIT_ERROR_NOT_READY`，不会留下部分操作。

调用方根据检查结果创建格式、尺寸、层数和 mip 数一致的 Texture。完成一个 mip 范围入队后，使用
现有同步或异步 Batch 提交、轮询和取消接口。正式资源何时替换旧资源仍由上游资产系统决定。

## 二进制布局 v1

所有整数均为无符号小端值，表内没有原生指针或平台相关对齐。Manifest 必须恰好由 80 字节头、
连续变体表和连续子资源表组成，尾部数据视为非法。

### 头（80 字节）

| 偏移 | 大小 | 字段 |
|---:|---:|---|
| 0 | 8 | Magic `GRNTEXA\0` |
| 8 | 4 | Schema，当前为 1 |
| 12 | 4 | Header Size，必须为 80 |
| 16 | 4 | Width |
| 20 | 4 | Height |
| 24 | 4 | Depth |
| 28 | 4 | Array Layers |
| 32 | 4 | Mip Levels |
| 36 | 4 | Texture Dimension |
| 40 | 4 | Variant Count |
| 44 | 4 | Subresource Count |
| 48 | 32 | 非零逻辑内容 ID |

### 变体项（每项 72 字节）

| 偏移 | 大小 | 字段 |
|---:|---:|---|
| 0 | 4 | Texture Format |
| 4 | 4 | 声明的 Texture Usage |
| 8 | 4 | First Subresource |
| 12 | 4 | Subresource Count |
| 16 | 8 | Payload Offset |
| 24 | 8 | Payload Size |
| 32 | 32 | 该变体负载的 SHA-256 |
| 64 | 8 | 保留，必须为零 |

每个变体必须恰好包含 `mip_levels × array_layers` 个子资源。变体按调用方偏好排序；选择算法不内置
“BC 优于 ASTC”等平台假设。

### 子资源项（每项 40 字节）

| 偏移 | 大小 | 字段 |
|---:|---:|---|
| 0 | 4 | Mip Level |
| 4 | 4 | Array Layer |
| 8 | 8 | 相对变体起点的 Data Offset |
| 16 | 8 | Data Size |
| 24 | 4 | Bytes Per Row；零表示紧密块行 |
| 28 | 4 | Rows Per Image；零表示紧密块行数 |
| 32 | 8 | 保留，必须为零 |

每个 mip/层组合只能出现一次，数据范围不能越过变体负载或彼此重叠。行布局必须符合对应格式的
块宽、块高和每块字节数。

## 所有权与线程安全

检查和选择调用不保留输入内存，可并发处理不同输出对象。逐 mip 入队遵循 Upload Batch 的线程
安全规则；同一个 Batch 和 Texture 的写入仍由调用方排序。文件读取、网络、缓存、离线转码和
离线转码不属于 Granit 运行时 API；资产工具可以使用公共编码接口生成 Manifest。
