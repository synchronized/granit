<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 资源值类型

`granit/renderer/resource_types.h` 定义 Buffer、Texture、Texture View、Sampler 与 Pipeline 共用的
后端无关值类型。本页说明这些值之间的关系；资源创建、能力和限制由对应领域 Reference 定义。

## 内存位置

`granit_memory_location` 表达 CPU/GPU 访问意图，不对应具体 Heap、Vulkan Memory Type 或 WebGPU
实现：

- `AUTOMATIC`：由 Granit 根据用途选择，不承诺能够映射。
- `DEVICE`：用于 GPU 高频访问，公共契约不允许映射。
- `UPLOAD`：CPU 写、GPU 读，可以映射并在写访问结束时处理 flush。
- `READBACK`：GPU 写、CPU 读，在 CPU 读取前处理 invalidate。

Buffer 的映射、上传和回读行为见 [Buffer](buffer.md)；Texture 的写入、读取和传输行为见
[Texture](texture.md)。

## 用途与格式

Buffer usage 是 transfer、vertex、index、uniform、storage 和 indirect 等操作的位集合。Texture
usage 独立表达 transfer、sampled、storage、color attachment 与 depth/stencil attachment。
创建资源时用途必须覆盖其后续参与的全部操作。

`granit_texture_format` 是 Granit 定义的颜色、深度/模板和块压缩格式，不等于 `VkFormat` 或
WebGPU 枚举。格式块 Footprint 描述紧密 CPU 数据布局；设备能力查询返回当前 Renderer 支持的
用途、过滤特性和样本数。完整格式、写入和 Mipmap 规则见 [Texture](texture.md)，多格式资产选择
见 [Texture Asset Manifest](texture-asset.md)。

## Texture 子资源与 View

Texture 描述包含维度、格式、用途、内存位置、尺寸、mip、数组层和样本数。View 通过
`granit_subresource_range` 选择 aspect、mip 和数组层范围，并可使用 component mapping 改变采样
结果的 RGBA 来源。

`GRANIT_REMAINING_MIP_LEVELS` 与 `GRANIT_REMAINING_ARRAY_LAYERS` 表示从起始位置延伸到资源末尾。
具体维度、样本数、Cube、默认 View、父子生命周期和格式重解释限制见 [Texture](texture.md)。

## 采样状态

共享值类型定义 nearest/linear filter、mipmap filter、寻址模式和 compare operation。
`granit_sampler_desc` 组合这些值以及各向异性、LOD bias 和 LOD 范围。设备限制、比较采样和
各向异性规则见 [Sampler](sampler.md)。

## 跨资源约束

- Sample count 同时用于 Texture、Render Target 和 Graphics Pipeline，三者必须匹配。
- Texture aspect 必须与格式一致；颜色格式不能作为深度/模板附件，反之亦然。
- Texture View 由父 Texture 创建并继承其 Renderer domain；销毁 Texture 会使全部 View 失效。
- 绑定、复制、渲染和提交入口会继续验证 usage、范围、格式、样本数和资源归属。

Render Target 的 load/store、clear 和 resolve 规则见 [Render Target Attachment](render-target.md)；
Pipeline 的顶点布局、绑定和状态规则见 [Graphics 与 Compute Pipeline](pipeline.md)。

## ABI 与扩展

- 创建描述以 `struct_size` 开头，调用方应使用对应初始化宏。
- 保留字段必须为零，未知枚举值和用途位会被拒绝。
- 后续字段只追加到结构尾部，不改变已有字段含义。
- C++20 入口提供强类型枚举和位运算，并在调用时映射到同一 C ABI 值。
- 当前稳定等级和结构扩展承诺见[版本与兼容策略](compatibility.md)。
