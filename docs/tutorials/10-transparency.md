<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 10：Transparency Gallery

本教程使用共享立方体 Mesh 和轨道相机展示 Opaque、Mask、Blend 以及多个透明对象的深度关系，
完整源码位于
[`examples/tutorials/10_transparency`](../../examples/tutorials/10_transparency)。

教程明确展示当前边界：Granit 提供对象级排序和标准 Alpha 混合，不提供逐三角形排序、透明阴影、
折射或 OIT。Mask 通过片元 Alpha discard 验证，Blend 使用普通 Alpha 因子；场景使用共享立方体
法线和可调方向光/环境光，面板还提供轨道相机和自动旋转，便于同时观察光照与深度关系。

```powershell
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug --target granit_tutorial_10_transparency
ctest --test-dir build/windows-clang-debug -R "^granit\.tutorial\.10_transparency$" --output-on-failure
```
