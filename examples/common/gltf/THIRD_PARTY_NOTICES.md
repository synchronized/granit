<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 模型查看器第三方通知

本文件只覆盖 `granit_example_gltf_support` 的私有依赖和可选示例资产。这些内容不进入 Granit
安装包或公共 ABI。

| 内容 | 锁定版本 | 许可证 | 用途 |
| --- | --- | --- | --- |
| cgltf | 1.15 / `360db1a95480fe102ae9c69b27c5d101167ff5ba` | MIT | glTF/GLB 解析 |
| stb_image | 2.30 / `013ac3beddff3dbffafd5177e7972067cd2b5083` | MIT | PNG/JPEG 解码 |
| FlightHelmet | `9429648735279342b4c32b8745f7904196607379` | CC0-1.0 | 手动模型验收 |
| Studio Small 03 | `d69ec09a43016714fd0dda163b3b0c585c968f56` | CC0-1.0 | 模型查看器环境光源 |

依赖源码和完整许可证分别来自：

- <https://github.com/jkuhlmann/cgltf>
- <https://github.com/nothings/stb>
- <https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/FlightHelmet>
- <https://polyhaven.com/a/studio_small_03>

FlightHelmet 的元数据文档使用 CC-BY-4.0；下载脚本保存的 `LICENSE.md` 是上游生成的完整模型
许可说明。模型文件本身为 CC0-1.0，且不进入默认 Git 工作树。

Studio Small 03 由 Greg Zaal 创作并由 Poly Haven 以 CC0-1.0 发布。仓库只保存来源和校验清单；
原始 HDR 与后续预处理产物均不进入 Granit 安装包。
