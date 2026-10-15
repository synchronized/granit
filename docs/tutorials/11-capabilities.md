<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 11：Capabilities

本教程可视化查询 Renderer limits 和 Texture Format capabilities，并实际尝试创建带 mip 链的
采样纹理、Sampler 和 Indirect Buffer，验证结果必须通过公共 API 得到，而不是由教程猜测后端。
完整源码位于
[`examples/tutorials/11_capabilities`](../../examples/tutorials/11_capabilities)。

不可用组合必须显示为不支持；教程不会伪造 BC、深度或跨后端格式对称性。资源创建失败只作为
探测结果显示，不阻止其它能力继续展示。它也不实现 OIT、SVT 或 GPU-driven 多批次，这些属于
后续独立设计。
