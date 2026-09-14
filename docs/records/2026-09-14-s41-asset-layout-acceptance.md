<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-14 S-41 资产目录收敛本地验收

## 结果

S-41 已完成 Granit 仓库内部资产布局收敛。作者输入位于 `assets/sources`，已提交的安装与内建
快照位于 `assets/generated`，Runtime 与 AssetTools 共用的格式实现位于 `src/asset_formats`，
测试输入位于 `tests/fixtures`。SDK 安装路径、公共 API、ABI、资产格式和资产内容字节均未改变。

旧 `src/pipeline/assets`、`src/pipeline/shaders`、`src/assets` 与 `tests/assets` 目录已移除。所有纯
资产迁移均由 Git 识别为 100% rename；锁定 Shader Toolchain 的快照比较测试继续通过。

## 本地验证

- Windows VS2022 共享 Debug：完整构建与 106/106 测试通过。
- Windows VS2022 静态 Debug：Toolchain `off` 下完整构建与 85/85 测试通过。
- 共享与静态安装结果均通过导出、公开资产、分组件和版本选择审计；匹配配置的独立 C11/C++20
  Consumer 各通过 7/7 测试。
- Emscripten Debug 完整构建与 3/3 Node 测试通过；已安装 SDK 的独立 Web Consumer 通过 3/3。
- 安装结果不包含 HLSL、SPIR-V、WGSL、`.grshidx.json`、`.inc` 或测试 Fixture。

## 后续验收

S-41 随 0.23.0 统一执行 Linux、远端安装 Consumer 与不可变 Release Candidate 验收。本记录不将
这些远端项目或 0.23.0 正式发布标记为完成。
