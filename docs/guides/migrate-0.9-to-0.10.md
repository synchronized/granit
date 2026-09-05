<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.9 迁移到 0.10

0.10.0 将环境贴图所有权提升到 RenderPipeline component，并把环境资产升级为带 payload
SHA-256 的 GRENV v3。Core Renderer 和标准 PBR Shader 契约保持不变。

## 使用公共 Environment Map

1. 包含 `granit/pipeline/environment_map.h` 或 `.hpp`。
2. 读取 `.grenv` 文件为内存字节，并调用 `granit_environment_map_create_from_asset`；缺少外部资产时
   可调用 `granit_environment_map_create_builtin`。
3. 通过 `granit_environment_map_get_info` 获取逐帧借用描述，复制后可调整强度和旋转。
4. 在销毁 Renderer 前销毁 Environment Map。

安装包的公共示例资产位于
`${granit_RENDER_PIPELINE_ASSET_DIR}/environments/studio_small_03.grenv`。

## 重新生成旧环境资产

GRENV v2 不再读取。使用仓库环境生成工具从锁定的 Irradiance、Prefiltered 和 BRDF LUT 输入重新
生成 v3；不要只修改版本字段，因为 v3 头还包含 payload SHA-256。迁移后应清理旧构建目录并将
`find_package(granit 0.9 ...)` 更新为 `find_package(granit 0.10 ...)`。

## ABI

本版本仅为 RenderPipeline 增加 Environment Map 导出和新结构，没有删除已有导出。Granit 仍处于
0.x，Consumer 应重新编译，不应把 C++ 包装视为二进制稳定接口。
