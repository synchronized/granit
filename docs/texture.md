<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Texture 与 Texture View

Texture 拥有图像存储，Texture View 描述如何访问其子资源。两者都是属于 Renderer 的独立 64 位
句柄。

当前支持单 mip、单数组层、单采样的 2D Texture。3D、Cube、数组、多 mip、多采样和格式重解释
已经能由资源值类型表达，但在实现完成前返回不支持。

`granit_texture_create_with_default_view` 可以原子创建 Texture 和完整范围 View。默认 View 继承
Texture 格式，并根据颜色、深度或深度模板格式自动选择 aspect。

销毁 View 不影响父 Texture；销毁 Texture 会使其全部 View 句柄立即失效。当前 Texture 不接收
初始像素数据，上传、Layout 转换和 mipmap 生成由后续任务实现。
