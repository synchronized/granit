<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 测试夹具

此目录只保存测试输入，不属于可安装或发布的 Granit 资产。

- `shaders/`：测试专用 HLSL 作者输入、固定 SPIR-V/WGSL 载荷和 Library 清单。
- `materials/`：测试专用 Material 源清单。
- `smoke/`：跨模块冒烟测试使用的最小工作负载。
- `generated/`：缺少 Shader Toolchain 时使用的已提交后端载荷快照。

测试生成的临时资产必须写入 Build Tree，不应提交到此目录。
测试 Shader 作者输入统一使用 HLSL；不保留已经有 HLSL 来源且未被测试读取的 GLSL 副本。
