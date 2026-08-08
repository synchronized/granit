<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 资源值类型

`granit/resource_types.h` 定义 Buffer、Texture、Texture View 和 Sampler 共用的后端无关值类型。
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

当前验证范围只接受单 mip、单 array layer、单 sample 的 2D Texture 和完整范围 2D View。
其他有效维度和采样数已经能够表达，但在对应实现完成前返回 `GRANIT_ERROR_UNSUPPORTED`。

像素格式数值由 Granit 定义，不等于 `VkFormat`。深度/模板格式不能作为颜色附件，颜色格式也
不能作为深度模板附件。

## Sampler

`granit_sampler_desc` 描述过滤、寻址、比较、各向异性和 LOD 范围。当前基础范围支持 nearest、
linear、repeat、mirrored repeat 和 clamp to edge；比较采样和各向异性留待设备能力接入。

## ABI 与扩展

- 创建描述以 `struct_size` 开头，调用者应填写对应的 `*_VERSION_1_SIZE`。
- 保留字段必须为零。
- 后续字段只追加到结构尾部，不改变已有字段含义。
- C++20 入口提供强类型枚举和位运算，并在调用时转换为 C ABI 描述结构。
- 当前项目尚未承诺稳定 ABI，最小渲染闭环完成前仍可能根据实现验证调整。
