<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 11：Capabilities

本教程可视化查询 Renderer limits 和 Texture Format capabilities，验证格式、Usage、Filterable、
采样器相关能力必须通过公共查询得到，而不是由教程猜测后端。完整源码位于
[`examples/tutorials/11_capabilities`](../../examples/tutorials/11_capabilities)。

不可用组合必须显示为不支持；教程不会伪造 BC、深度或跨后端格式对称性。它也不实现 OIT、SVT 或
GPU-driven 多批次，这些属于后续独立设计。
