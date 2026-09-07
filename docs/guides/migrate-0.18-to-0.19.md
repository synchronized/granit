<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.18 迁移到 0.19

0.19.0 在既有压缩纹理与格式能力之上增加 Texture Asset Manifest、变体选择和逐 mip 上传。
0.18 的直接 Texture 创建与上传接口保持原语义；0.x Consumer 应重新编译，并把 CMake 请求版本
更新为 0.19。

## 生成与检查 Manifest

资产工具使用 `granit_texture_asset_encode` 把逻辑尺寸、内容 ID、按偏好排序的格式变体和完整
mip/层布局编码为 v1 Manifest。运行时用 `granit_texture_asset_inspect` 两阶段取得数量与元数据，
不要读取固定二进制偏移。

## 选择和上传变体

使用 `granit_renderer_select_texture_asset_variant` 根据真实设备格式能力和用途选择变体。选择结果
为 `UNSUPPORTED` 时由上游选择另一份资产或报告错误，Granit 不生成隐式 RGBA8 回退。

调用方按检查结果创建 Texture，再用 `granit_upload_batch_write_texture_asset_mips` 将指定 mip 范围
加入空 Upload Batch。之后继续使用既有同步或异步提交、背压重试、取消和完成状态接口；文件读取、
缓存、网络和正式资源切换仍由上游负责。
