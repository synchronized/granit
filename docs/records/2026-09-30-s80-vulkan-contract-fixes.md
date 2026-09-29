<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-30 S-80 Vulkan 契约修复验收

## 范围

本记录覆盖 Gneiss `UPSTREAM-043` 的 U43-06/U43-07，不新增 PBR 材质能力。

- U43-06：MASK 阴影顶点阶段读取 `uv1_mask`，MaterialConstants 必须对顶点和片元阶段可见；
- U43-07：标准 Alpha Cutoff Shader 使用 Demote 指令，Vulkan 设备必须先查询并启用对应特性。

## 修复

- `material_template_gpu.cpp` 将 MaterialConstants 的可见性设为 Vertex | Fragment，覆盖 MASK 阴影的
  UV1 选择路径；
- Vulkan 物理设备候选查询 `shaderDemoteToHelperInvocation`，设备选择拒绝不支持的设备；
- Vulkan 逻辑设备创建链启用已确认支持的 Demote 特性；
- 增加设备选择负向测试和运行时能力状态断言。

## 本地验证

Windows Clang Debug：

- `granit.backend.vulkan`、`granit.backend.capabilities`、`granit.backend.resources`：通过；
- `granit.lighting.pbr_render`、`granit.pipeline.render`、`granit.pipeline.api`、
  `granit.material.pbr_adapter`、`granit.material.assets`：通过；
- `granit.documentation.links`：通过。

标准 Shader 仍保留原有 Alpha Cutoff 语义和像素路径；不支持 Demote 的设备不会进入 Granit 可用设备
集合，因此不会以未声明特性创建逻辑设备。远端 PR 矩阵已通过 Linux、Windows、Emscripten 浏览器和
安装 SDK 检查；Linux Vulkan 集成验证、MASK/UV1/顶点色模型路径及原有像素测试均通过。浏览器 runtime
首次因 Dawn 设备瞬时丢失失败，重跑后通过，未发现本次 Vulkan 契约修复引入的回归。
