<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# R-10C：Mipmap 生成

## 状态

- 设计状态：已确认实现顺序
- 实现状态：R-10C0～R-10C1 已完成
- 父计划：[R-10 通用资源传输](R-10-resource-transfer.md)

## 目标

为显式 Command Recorder 提供受设备能力门禁的 mipmap 生成命令。首版通过线性 Blit 从上一
级生成下一级，不隐式创建 Texture，也不处理法线重归一化、Alpha Coverage 或离线滤波。

## 已发现的前置约束

当前 Recorder 和 Renderer 的图像状态缓存以原生图像为键，同一图像只保留一个布局与访问
状态。普通渲染和整图复制可以满足该假设，但 mipmap 生成要求同一 Texture 的不同 mip 在同一
时刻分别处于 `TRANSFER_SOURCE` 和 `TRANSFER_DESTINATION` 状态。

如果直接录制 Blit，可能出现以下问题：

- 将目标 mip 错误地当作已经拥有源 mip 的旧布局；
- 为整张图像应用只适合单个 mip 的屏障；
- 后续读取或采样时丢失部分 mip 的最终状态；
- Vulkan 验证层报布局不匹配，或在无验证层环境产生未定义结果。

因此必须先把状态键扩展为“图像 + aspect + mip 范围 + 数组层范围”，不能通过硬编码布局或
关闭验证层绕过。

## 实施顺序

### R-10C0：子资源状态跟踪

- Recorder 按图像子资源范围记录初始和最终访问状态。
- Renderer 提交前按相交范围查找旧状态，并对范围执行拆分或合并。
- 不相交的 mip 和数组层可以拥有不同布局。
- 完全相同且状态一致的相邻范围可以合并，避免状态表无限增长。
- 保持现有整图渲染、Texture 复制、回读和 Swapchain 路径行为不变。

当前实现先将范围展开为单位 aspect/mip/layer 状态，以获得直接且可验证的正确性。相邻同状态
范围合并保留为测量驱动的优化；只有状态表或屏障数量形成可测瓶颈时再增加区间压缩。

### R-10C1：格式能力门禁

- 将 Granit Texture Format 映射为内部 Vulkan Format。
- 查询最优平铺格式是否同时支持 Blit Source、Blit Destination 和线性过滤。
- 压缩、深度/模板、整数及不支持线性过滤的格式返回 `UNSUPPORTED`。
- 能力查询保留在内部，不向公共 API 暴露 Vulkan 标志。

已实现基于 `VkFormatProperties3::optimalTilingFeatures` 的内部查询，并要求 Blit Source、Blit
Destination 与线性过滤三项能力同时存在。公共命令将在 R-10C2 使用该门禁。

### R-10C2：公共命令

- 输入 Texture、base mip、level count、base array layer 和 layer count。
- Texture 必须为单采样，并同时声明 Transfer Source 与 Transfer Destination 用途。
- `level_count` 至少为 2，范围不得越过 Texture 已创建的 mip 和数组层。
- 每一级以 `max(1, 上一级尺寸 / 2)` 计算目标尺寸，支持非二次幂纹理。
- Recorder 在提交完成前保活 Texture。

### R-10C3：验证

- 使用具有明确颜色分区的 mip 0，生成后回读非零 mip。
- 覆盖非二次幂尺寸、部分 mip 范围、数组层、越界和缺少用途。
- 动态库与静态库均运行验证层测试。
- 文档明确运行时生成适用范围，以及离线资产 mipmap 的质量优势。

## 首版非目标

- Compute Shader 降采样。
- 法线贴图重归一化和高度图专用过滤。
- Alpha Coverage 保持。
- Gamma、颜色空间或通道转换。
- 自动为只有一级 mip 的 Texture 重新分配存储。
- 在一个调用中处理多个 Texture。

## 验收标准

- 不同 mip 的布局和访问状态可独立追踪，且现有 Texture 路径没有回归。
- 不支持线性 Blit 的设备或格式稳定返回 `UNSUPPORTED`。
- 支持的 Texture 可以在一个 Recorder 中生成指定范围，并回读得到正确尺寸与像素。
- 公共 C ABI 不暴露 Vulkan 类型，C++20 包装保持轻量。
