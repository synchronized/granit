<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# ResourceTableGallery

这是 Tutorial 12 的可控多材质验证资产，由
[`tools/generate_resource_table_gallery.py`](../../../../../tools/generate_resource_table_gallery.py)
使用 Blender 无界面模式生成。默认生成 16 个材质；可通过 `--count` 生成其它确定性压力资产，
例如 `--count 32` 或 `--count 64`。

当前权威资产包含 32 个对象、32 个材质、32 张 BaseColor 纹理和 32 张 MetallicRoughness 纹理。Tutorial
12 会将 32 张 BaseColor 纹理写入 Resource Table 的连续槽位，作为多材质索引验证负载；实际默认
绘制仍由标准材质路径负责。模型和纹理均为本仓库生成内容，不依赖外部模型或下载服务。重新生成
后，应保持 glTF、BIN 和 PNG 文件一起更新，并通过 Tutorial 12 smoke test。当前提交的权威资产仍为
32 材质；更大规模资产需要单独评估包体、浏览器下载和测试时间后再接入教程。

