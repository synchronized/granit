<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Renderer 后端能力矩阵

本文档是 Vulkan 与浏览器 WebGPU 公共能力差异的权威参考。矩阵只描述当前已经实现并验证的行为；
计划中的能力在实现和测试完成前仍标记为不支持。

`GRANIT_ERROR_UNSUPPORTED` 表示请求本身可以由公共 API 表达，但当前后端、设备、构建目标或参数
组合无法满足。调用方能够提前查询的能力必须先查询；没有查询入口的限制以本矩阵和对应功能参考
为准。Granit 不会静默替换格式、降低采样数或模拟语义不同的能力。

## 核心能力

| 公共能力 | Vulkan 桌面 | 浏览器 WebGPU | 查询或失败语义 |
|---|---|---|---|
| Renderer 生命周期与资源域 | 支持 | 支持 | 初始化失败返回对应结果码 |
| Buffer 创建、上传与回读 | 支持 | 支持 | 内存位置与 Usage 不兼容时返回不支持 |
| 2D 与 Cube Texture | 支持 | 支持 | 使用 Texture 格式能力查询 |
| 1D、3D、普通数组与 Cube Array | 不支持 | 不支持 | 创建返回不支持 |
| 压缩 Texture | 设备相关 | Feature 相关 | 使用 Texture 格式能力查询 |
| 多采样 Texture | 设备与格式相关 | 当前支持 1x/4x 子集 | 检查 Renderer Limits 和格式能力 |
| Sampler 各向异性 | 设备相关 | 设备相关 | 检查 `max_sampler_anisotropy` |
| Sampler LOD Bias | 设备限制内支持 | 不支持 | WebGPU 创建返回不支持 |
| Upload Batch 与异步上传 | 支持 | 支持 | Usage、范围或格式不兼容时返回不支持 |
| 异步 Readback | 支持 | 支持 | 检查 `ASYNC_READBACK` 能力位 |
| Buffer/Texture 复制与 Buffer Fill | 支持 | 支持 | Usage、范围和格式规则见 Command Recorder |
| 运行时 Mipmap 生成 | 支持线性 Blit 的格式 | 支持可过滤颜色格式 | 不兼容格式返回不支持 |

## Shader、绑定与 Pipeline

| 公共能力 | Vulkan 桌面 | 浏览器 WebGPU | 查询或失败语义 |
|---|---|---|---|
| Vertex、Fragment、Compute Shader | 支持 | 支持 | 阶段与 Pipeline 类型不匹配返回无效参数 |
| Shader Library 逻辑名称 | 选择 SPIR-V 变体 | 选择 WGSL 变体 | 没有兼容变体返回不支持 |
| 直接 Shader 输入 | 仅 SPIR-V | 仅 WGSL | 格式与后端不匹配返回不支持 |
| Uniform、Storage Buffer | 支持 | 支持 | 受绑定范围和对齐限制 |
| 采样 Texture、Depth Texture、Sampler | 支持 | 支持 | Layout 与资源类型不匹配返回无效参数 |
| Storage Texture Binding | 支持 | 支持 Write-Only RGBA8/RGBA16F | Read-Only 与 Read-Write 返回不支持 |
| Binding 资源数组 | 支持 | 不支持 | WebGPU 要求 `array_count == 1` |
| Dynamic Uniform Buffer | 支持 | 支持 | Offset 必须满足设备对齐 |
| Graphics Pipeline | 支持 | 支持当前子集 | 具体差异见下表 |
| Compute Pipeline 与 Dispatch | 支持 | 支持 | Dispatch 必须位于 Rendering 区域外 |
| 单次 Draw/Indexed Draw/Dispatch Indirect | 支持 | 支持 | Buffer 必须声明 Indirect Usage |
| Pipeline Warmup | 支持 | 支持 | 检查 Warmup 能力位 |
| Pipeline Cache 导入与导出 | 支持 | 不支持 | WebGPU 返回不支持 |
| 标准 PBR Opaque/Mask/Blend | 支持 | 支持 | 自动路径统一执行 Alpha 分类、对象级排序和预乘混合 |

当前 Graphics Pipeline 差异：

| 状态 | Vulkan 桌面 | 浏览器 WebGPU |
|---|---|---|
| Primitive Topology | Point、Line、Triangle 全部公开拓扑 | 全部拓扑；Indexed Strip 暂不支持 |
| Polygon Mode | Fill；Line/Point 取决于设备 | 仅 Fill |
| Cull Mode | None、Front、Back、Front And Back | 不支持 Front And Back |
| 颜色附件数量 | 最多 `GRANIT_MAX_COLOR_ATTACHMENTS`，受设备限制 | 支持 MRT，受 `max_color_attachments` 限制 |
| 颜色格式 | 使用格式能力查询 | R8、RG8、RGBA8、RGBA8 sRGB、BGRA8、BGRA8 sRGB、RGBA16F |
| 深度模板格式 | 使用格式能力查询 | D16 UNORM、D32 Float；不支持模板操作 |
| 样本数 | 使用 Renderer Limits 与格式能力查询 | 当前 Pipeline 支持 1x/4x |
| 分层 Rendering | 支持适用配置 | 当前 `layer_count` 必须为 1 |
| Attachment Load Discard | 支持 | 当前不支持颜色或深度 Load Discard |

Texture 格式能力仍是创建 Texture 的最终依据。格式能够作为 Texture 创建，不表示当前后端已经允许
它进入 Graphics Pipeline；Pipeline 子集限制按本表处理。

[Deferred Rendering 教程](../tutorials/07-deferred.md)使用三个颜色附件和一个 Depth Attachment，
端到端验证 Vulkan 与浏览器 WebGPU 的 MRT、中间纹理采样和连续 Pass。

## 命令、诊断与呈现

| 公共能力 | Vulkan 桌面 | 浏览器 WebGPU | 查询或失败语义 |
|---|---|---|---|
| Graphics/Compute Command Recorder | 支持 | 支持 | Renderer 未就绪返回未就绪 |
| Timestamp Query | 支持 | 当前浏览器契约不支持 | 检查 Timestamp 能力位 |
| GPU 对象命名 | 启用 Validation 与 Debug Utils 时支持 | 不支持 | 不可用时返回不支持 |
| Win32 Surface | Windows 构建支持 | 不属于正式浏览器后端 | 来源不匹配返回不支持 |
| XCB/Wayland Surface | 对应 Linux 选项启用时支持 | 不属于正式浏览器后端 | 来源不匹配返回不支持 |
| Canvas Surface | 不支持 | 支持 | Vulkan 返回不支持 |
| Swapchain Acquire、Present 与 Resize | 支持 | 支持 | 表面能力不兼容时返回不支持 |

公共 `surface_type` 描述的是可表达的来源集合，不表示每个后端都支持所有来源。普通应用优先通过
Window component 创建 Surface；外部窗口集成再使用 Native Surface 描述。

## 固有限制

以下差异没有可靠的跨后端等价语义，当前不会为了接口表面对称而模拟：

- 浏览器 WebGPU 不提供 Vulkan Pipeline Cache 数据导入导出的同等契约。
- WebGPU Sampler 没有 LOD Bias 字段。
- WebGPU 没有 Line/Point Polygon Mode 或 Front And Back Cull Mode。
- 浏览器 WebGPU 不能满足 Granit 当前“任意命令位置写入”的 Timestamp 契约。
- Vulkan 不消费浏览器 Canvas selector，浏览器 WebGPU 不消费桌面原生窗口句柄。
- Vulkan 的显式 Barrier、原生同步对象和 WebGPU 的隐式状态跟踪都不进入公共 API。

具体参数、所有权和调用顺序继续以对应的
[Renderer](renderer.md)、[Texture](texture.md)、[Shader](shader.md)、[Pipeline](pipeline.md)、
[Command Recorder](command-recorder.md) 和 [Surface](surface.md) 参考为准。
