<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 数学值类型

`granit/math/types.h` 提供供 C ABI、Scene 和高级渲染接口共同使用的普通数据类型：

- `granit_float2`、`granit_float3`、`granit_float4`。
- 列主序、包含 16 个 `float` 的 `granit_matrix4`。

C++20 入口 `granit/math/types.hpp` 在 `granit::math` 命名空间提供同一类型的别名和单位矩阵常量。
C 与 C++ 使用完全相同的对象布局，不需要在动态库边界复制为另一种数学结构。

`granit/math/functions.hpp` 提供 header-only 的最小 C++20 渲染数学函数：

- 三维向量加减、标量乘法、点积、叉积、长度、归一化与有限性检查；
- 列主序矩阵乘法、向量变换和带透视除法的点变换；
- 平移、缩放和 X/Y/Z 轴旋转矩阵；
- `look_at_rh`、`perspective_rh_zo` 与 `orthographic_rh_zo`。

这些函数使用项目统一的右手坐标系、列主序矩阵和 `[0,1]` 深度范围。可能失败的 View、投影与
点变换使用 `bool + output`：失败时不修改输出。零向量或非有限向量归一化为零向量。

这组函数不增加 C ABI 导出，也不试图替代完整数学库。公共层不提供 Quaternion、Transform 层级、
SIMD、几何容器或相机控制器；使用者仍可采用 GLM、DirectXMath 或自有数学库，并在调用边界显式
转换。准确坐标含义见[坐标系统约定](coordinates.md)。
