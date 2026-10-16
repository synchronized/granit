<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 13：Vertex Streams

本教程对应 bgfx 的 `34-mvs` 能力探针，验证同一套位置、法线、UV 和颜色数据在拆分与交错
Vertex Buffer 布局下都能正确渲染。源码位于
[`examples/tutorials/13_vertex_streams`](../../examples/tutorials/13_vertex_streams)。

## 验证内容

- 拆分布局：Vertex Buffer 0 为 `POSITION/NORMAL`，Vertex Buffer 1 为 `TEXCOORD_0/COLOR_0`；
- 交错布局：一个 Vertex Buffer 包含四种属性；
- 实例布局：在上述两个顶点流之后增加独立的 `step_mode=instance` Vertex Buffer，
  一次 Index Draw 绘制三个实例；
- 两种布局共用同一个 36 个 `uint16` Index Buffer；
- UI 可实时切换布局和实例流，便于比较顶点绑定和光照结果；
- 所有 Vertex Buffer 都在 `begin_rendering` 前绑定，Rendering 过程中不切换顶点缓冲区。

教程使用 `examples/common/geometry/primitives.hpp` 的程序化立方体，不新增公共 ABI，
也不需要外部模型或纹理。

## 构建与验证

```powershell
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug --target granit_tutorial_13_vertex_streams
ctest --preset windows-clang-debug `
  -R "^granit\.tutorial\.13_vertex_streams$" --output-on-failure
```

教程的拆分布局是验证 Vertex Buffer 多流绑定的最小用例，实例流则验证顶点流与实例流可以
同时绑定；动态更新和更复杂的资源表组合属于后续独立能力探针。
