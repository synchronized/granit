<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-13 S-38 发布验收

## 范围

本次验收覆盖 0.22.0 AssetTools SDK、统一 CLI、Shader Toolchain 获取模块、版本元数据、安装导出、
安装 Consumer 与跨平台 Release Candidate。

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

## 远端验证

- [Linux](https://github.com/synchronized/granit/actions/runs/34757891005)：GCC/Clang、共享/静态、
  安装 Consumer、运行时 Integration 和 Shader Toolchain 全部通过。
- [Windows](https://github.com/synchronized/granit/actions/runs/34758190229)：MSVC 共享/静态、安装
  Consumer 和 Shader Toolchain 全部通过。
- [Emscripten](https://github.com/synchronized/granit/actions/runs/34758194517)：静态安装 Consumer、
  浏览器平台、ImGui 和 Model Viewer 通过。ImGui 首次遇到无头 Chrome 等待超时，同提交重跑通过。
- [Documentation](https://github.com/synchronized/granit/actions/runs/34758199028) 与
  [Quick Check](https://github.com/synchronized/granit/actions/runs/34758203566) 通过。
- [Release Candidate](https://github.com/synchronized/granit/actions/runs/34757895153)：Windows/Linux
  共享与静态四套 SDK、安装审计、校验和与候选 manifest 全部通过；未创建公开 Release。
