<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 变更记录

本文件记录面向使用者的公共接口、行为、构建和兼容性变化。项目当前仍处于 0.x；`Unreleased`
内容不代表已经发布。版本兼容规则见[版本与兼容策略](docs/reference/compatibility.md)。

## Unreleased

当前没有尚未归入版本的公共变更。

## 0.2.0 - 2026-08-24

### 新增

- 核心 Renderer C ABI、C++20 RAII 包装及 Windows/Linux 共享、静态安装 Consumer 验证。
- RenderPipeline、Window、Input、SDL3 Integration 和 ImGui Integration component。
- Granit 0.1.0 基线上的 Core、RenderPipeline、Window 和 Input C ABI 回归快照，以及
  component 级所有权、错误、线程和扩展契约。

### 修复

- RenderPipeline 的公开描述结构提供固定 V1 尺寸，未知尾部可以按统一规则忽略。
- Input 事件与状态输出按调用方 `struct_size` 容量写入，避免旧结构缓冲区越界。
- 核心 Pipeline 销毁接口统一为空句柄返回 `GRANIT_ERROR_INVALID_HANDLE`。

### 兼容性

- 0.2.0 仍是非稳定版本；0.x 次版本可包含有迁移说明的破坏性变更。
