<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Shader 后端载荷夹具

此目录保存测试专用的 SPIR-V/WGSL 快照，使缺少 DXC 或 Tint 的构建仍能验证运行时后端、格式解码和
错误路径。它们不是 Shader 作者输入，也不会安装或发布；正式资产只从 `assets/shaders` 和
`src/pipeline/shaders` 中的 HLSL Library 源清单构建。
