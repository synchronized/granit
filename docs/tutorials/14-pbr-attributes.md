<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 14：PBR 顶点属性

本教程对应 bgfx 的 `06-bump` 所关注的材质/纹理验证方向，但使用 Granit 已有的 CC0 Suzanne
资产。源码位于 [`examples/tutorials/14_pbr_attributes`](../../examples/tutorials/14_pbr_attributes)。

## 验证内容

教程通过现有 Render Pipeline 渲染 Suzanne，并在面板中报告模型实际提供的：

- `POSITION` 与 `NORMAL`；
- `TANGENT`，用于切线空间法线贴图；
- `TEXCOORD_0`，用于 Base Color 和 Metallic/Roughness 纹理；
- 可选的 `TEXCOORD_1`，用于扩展材质通道。

Suzanne 当前使用 Base Color、Metallic/Roughness 和标准 PBR 默认法线纹理；因此教程重点是
验证完整顶点属性进入上传、Mesh 和 PBR 管线的路径，而不是引入新的外部 bump 贴图。纯色
Base Color、Metallic 和 Roughness 仍可通过面板实时修改。

## 资源与构建

教程复用 `examples/assets/tutorials/02_pbr_assets` 下的 Suzanne 及其两张纹理，不复制 bgfx
资源，也不新增公共 API。

```powershell
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug --target granit_tutorial_14_pbr_attributes
ctest --preset windows-clang-debug `
  -R "^granit\.tutorial\.14_pbr_attributes$" --output-on-failure
```

浏览器构建使用同一资产清单和 WebGPU Render Pipeline，后续跨平台验收记录以版本文档为准。
