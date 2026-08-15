<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 资源值类型

`granit/renderer/resource_types.h` 定义 Buffer、Texture、Texture View 和 Sampler 共用的后端
无关值类型。
这些结构目前用于冻结资源描述和验证规则；资源创建、销毁及映射函数将在 R-03 至 R-06 中加入。

## 内存位置

- `AUTOMATIC`：由 Granit 根据用途选择，不承诺能够映射。
- `DEVICE`：用于 GPU 高频访问，公共契约不允许映射。
- `UPLOAD`：CPU 写、GPU 读，允许映射并在写访问结束时处理 flush。
- `READBACK`：GPU 写、CPU 读，在 CPU 读取前处理 invalidate。

这些值是访问意图，不对应具体 Heap 或 Vulkan Memory Type。

## Buffer

`granit_buffer_desc` 包含字节大小、内存位置和可组合用途。首版用途覆盖 transfer、vertex、
index、uniform、storage 和 indirect。大小与用途必须非零，未知用途位会被拒绝。

## Texture 与 View

`granit_texture_desc` 描述存储，包含维度、格式、用途、尺寸、mip、数组层和采样数。
`granit_texture_view_desc` 描述访问同一存储的子资源范围。View 是独立资源，父 Texture 由未来
创建函数单独传入。

View 还可独立映射 RGBA 输出分量，支持保持原分量、常量零/一以及读取任一源分量。例如文字
图集可将 R8 覆盖率映射为 `(1, 1, 1, R)`，继续复用通用 Canvas Shader，并由顶点颜色提供文字
颜色。分量映射只改变采样结果，不复制或修改 Texture 存储。

当前支持单 sample 的单层 2D Texture，以及单个六面 Cube Texture。两者均可包含不超过完整链的
mip；View 可以选择连续 mip 范围，2D View 固定单层，Cube View 固定六层。Cube 的宽高必须相等、
depth 必须为 1、array layer 必须为 6。Cube Array、1D、3D 和多采样仍返回
`GRANIT_ERROR_UNSUPPORTED`。

Cube 存储使用六个数组层，层顺序遵循正 X、负 X、正 Y、负 Y、正 Z、负 Z。调用方可以通过
Texture 写入接口指定单个面和 mip；创建完整 Cube View 后由 Shader 使用方向向量采样。Granit
当前不自动生成 mip，也不负责从经纬度环境图卷积为 IBL 资源。

销毁 Texture 会先级联销毁其全部 View，并立即使旧 View 句柄失效。验证模式下，如果仍存在
用户创建的 View，该级联操作会输出生命周期警告，但仍返回成功。

像素格式数值由 Granit 定义，不等于 `VkFormat`。深度/模板格式不能作为颜色附件，颜色格式也
不能作为深度模板附件。

## Sampler

`granit_sampler_desc` 描述过滤、寻址、比较、各向异性和 LOD 范围。当前基础范围支持 nearest、
linear、repeat、mirrored repeat 和 clamp to edge；比较采样和各向异性留待设备能力接入。

## Render Target Attachment

颜色和深度/模板附件定义在 `granit/renderer/render_target.h`，统一接收 Texture View。离屏 View 与
Swapchain Backbuffer View 不使用两套描述。详见 [Render Target Attachment](render-target.md)。

## ABI 与扩展

- 创建描述以 `struct_size` 开头，调用者应填写对应的 `*_VERSION_1_SIZE`。
- 保留字段必须为零。
- 后续字段只追加到结构尾部，不改变已有字段含义。
- C++20 入口提供强类型枚举和位运算，并在调用时转换为 C ABI 描述结构。
- 当前项目尚未承诺稳定 ABI，最小渲染闭环完成前仍可能根据实现验证调整。
