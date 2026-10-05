<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 10：Transparency

本教程以最小四边形场景验证 Opaque、Mask、Blend 和对象级透明排序。完整源码位于
[`examples/tutorials/10_transparency`](../../examples/tutorials/10_transparency)。

教程明确展示当前边界：Granit 提供对象级排序和标准 Alpha 混合，不提供逐三角形排序、透明阴影、
折射或 OIT。Mask 通过片元 Alpha discard 验证，Blend 使用预乘前的普通 Alpha 因子。

```powershell
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug --target granit_tutorial_10_transparency
ctest --test-dir build/windows-clang-debug -R "^granit\.tutorial\.10_transparency$" --output-on-failure
```
