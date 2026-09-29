<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.40 迁移到 0.41

0.41 更新了标准 PBR Material 与 Shader Library。只使用底层 Renderer 的项目无需额外迁移；消费
标准 PBR 资产的项目需要重新生成或替换材质资产。

## 更新标准 PBR 资产

1. 使用 0.41 SDK 中配套的 `materials/pbr_standard.grmat` 和
   `libraries/pbr_standard.grshlib`，不要混用旧版本文件。
2. 自行构建 Material 时，以新的 `assets/sources/materials/pbr_standard.grmat.json` 为源重新运行
   Material Tool。
3. 创建 Draw Binding 时，通过 `granit_pbr_material_variant_key` 传入真实 Alpha 模式、双面、UV1
   和顶点色语义；BLEND Variant 由自动 RenderPipeline 分入透明阶段。

旧模板版本为 8，新模板版本为 9。可以通过 `GRANIT_PBR_MATERIAL_TEMPLATE_VERSION` 和
`GRANIT_PBR_MATERIAL_CONTENT_HASH_HEX` 在打包或启动检查中拒绝混用资产。
