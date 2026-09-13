<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-13 S-38 本地发布验收

## 范围

本次验收覆盖 0.22.0 AssetTools SDK、统一 CLI、Shader Toolchain 获取模块、版本元数据、安装导出
和安装 Consumer。远端 Linux 与发布工作流不在本地记录的通过范围内。

## 结果

| 项目 | 配置 | 结果 |
|---|---|---|
| 完整测试 | Windows Clang Shared Release | 98/98 通过 |
| 完整测试 | Windows Clang Static Release | 88/88 通过 |
| 安装导出审计 | Shared/Static Release | 通过 |
| 安装 Component Consumer | Shared/Static Release | 通过 |
| glTF 回归复验 | Shared Release | 干净重编译后连续 10 次通过 |

安装 Consumer 覆盖 Core、RenderPipeline、Window、Input 和 AssetTools 的独立选包，并验证 0.22.0
精确版本、旧次版本、新次版本、未知 component 与已删除 ShaderTools component 的失败行为。

## 后续验收

S-38G 仍需在远端完成 Linux 共享/静态构建、安装 Consumer、Shader 双后端工具链和发布工作流，
通过后才能将计划状态改为已完成。
