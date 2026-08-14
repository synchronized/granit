<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 数学值类型

`granit/math/types.h` 提供供 C ABI、Scene 和高级渲染接口共同使用的普通数据类型：

- `granit_float2`、`granit_float3`、`granit_float4`。
- 列主序、包含 16 个 `float` 的 `granit_matrix4`。

C++20 入口 `granit/math/types.hpp` 在 `granit::math` 命名空间提供同一类型的别名和单位矩阵常量。
C 与 C++ 使用完全相同的对象布局，不需要在动态库边界复制为另一种数学结构。

`math` 目录只表达数学数据的归属和命名空间，不表示 Granit 要建设或要求使用一套大型数学库。
公共类型不提供通用向量运算、四元数、Transform、SIMD 或几何容器；使用者可以继续采用 GLM、
DirectXMath 或自有数学库，并在调用边界显式转换。

内部高层模块在这些值类型上共享最小运算实现，包括向量运算、右手 View 矩阵以及 Vulkan `[0,1]`
深度范围的投影矩阵。这些运算目前属于内部实现，不构成公共 C ABI。
