<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 10：Transparency Gallery

本教程使用共享立方体 Mesh 和轨道相机展示 Opaque、Mask、Blend 以及多个透明对象的深度关系，
完整源码位于
[`examples/tutorials/10_transparency`](../../examples/tutorials/10_transparency)。

教程明确展示当前边界：Granit 提供对象级排序和标准 Alpha 混合，不提供逐三角形排序、透明阴影、
折射或 OIT。Mask 通过片元 Alpha discard 验证，Blend 使用普通 Alpha 因子；场景使用共享立方体
法线和可调方向光/环境光；默认开启的 Gizmo 用黄色平面和箭头显示光源参考位置及光线传播方向。
面板还提供轨道相机和自动旋转，便于同时观察光照与深度关系。
`Normal debug` 选项会直接显示插值法线颜色，可用于区分法线、索引绕序和光照参数问题。
`Diffuse debug` 会用灰度直接显示 `normal · light_direction`，排除材质颜色造成的视觉误判。
`Opaque only debug` 会关闭 Mask 丢弃和 Blend 排序，强制所有对象走深度写入的不透明路径，用于
区分透明效果与深度/绕序问题。
`Cull none debug` 会临时关闭背面剔除并重建管线，用于判断缺面是否由索引绕序造成。

```powershell
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug --target granit_tutorial_10_transparency
ctest --test-dir build/windows-clang-debug -R "^granit\.tutorial\.10_transparency$" --output-on-failure
```
