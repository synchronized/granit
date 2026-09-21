<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 06：添加材质与光照

本章使用 `.grmat` 材质归档描述 Shader、参数和纹理需求，并增加方向光与基础 PBR。完成后，模型颜色、
粗糙度和金属度由材质数据驱动，旋转模型时可以观察连续的明暗变化。

## 1. 构建材质资产

材质源清单引用 Shader Library 中的稳定 Content ID，声明 `base_color`、`metallic`、`roughness` 和
纹理槽。AssetTools 在构建期生成 `.grmat`；应用运行时加载归档，不解析 HLSL 或工具专用中间文件。

## 2. 创建 Material

Material 创建时接收材质归档和对应 Shader Library。归档字节只需在创建调用期间有效；Library 句柄
必须按公共契约保持有效。参数通过稳定 Parameter ID 更新，类型和字节数必须与材质 Schema 一致。

## 3. 提供光照数据

加入一个朝向模型的白色方向光，并为相机提供世界空间位置。法线使用正确的变换矩阵；非均匀缩放时
不能直接用 Model Matrix 变换法线。

本章只使用直接光，先不加入阴影和环境贴图。这样可以区分材质参数、几何法线和高层渲染特性的职责。

## 4. 验收

- 修改 `base_color` 会改变固有色。
- 修改 `roughness` 会改变高光宽度，修改 `metallic` 会改变反射响应。
- 旋转模型或光源时明暗连续变化。
- 材质缺少所需 Shader 或参数类型错误时返回明确错误。

资产和运行时规则见 [Material](../reference/material.md)、[Shader Library](../reference/shader-library.md)
和 [AssetTools](../reference/asset-tools.md)。下一章把手工渲染步骤迁移到参考 Render Pipeline。

[上一章：组织 Mesh](05-mesh.md) · [下一章：使用 Render Pipeline](07-render-pipeline.md)
