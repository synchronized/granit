<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# ResourceTableGallery

这是 Tutorial 12 的可控多材质验证资产，由
[`tools/generate_resource_table_gallery.py`](../../../../../tools/generate_resource_table_gallery.py)
使用 Blender 无界面模式生成。

资产包含 16 个对象、16 个材质、16 张 BaseColor 纹理和 16 张 MetallicRoughness 纹理。Tutorial
12 会将 16 张 BaseColor 纹理写入 Resource Table 的连续槽位，作为多材质索引验证负载；实际默认
绘制仍由标准材质路径负责。模型和纹理均为本仓库生成内容，不依赖外部模型或下载服务。重新生成
后，应保持 glTF、BIN 和 PNG 文件一起更新，并通过 Tutorial 12 smoke test。

