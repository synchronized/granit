<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 变更记录

本文件记录面向使用者的公共接口、行为、构建和兼容性变化。项目当前仍处于 0.x；`Unreleased`
内容不代表已经发布。版本兼容规则见[版本与兼容策略](docs/reference/compatibility.md)。

## Unreleased

0.3.0 正在规划和开发；公共变更将在实现后记录于此。

### 修复

- RenderPipeline component 的创建接口把空 Renderer 统一归类为
  `GRANIT_ERROR_INVALID_HANDLE`，并在失败时保持输出句柄为零。
- Buffer、Command Recorder、Frame Context、Sampler、Texture 和 Timestamp Query Pool 的创建接口
  同步采用相同的空 Renderer 语义，C++ 包装与 C 接口保持一致。
- Surface、Swapchain、底层 Pipeline、Window 和 Input 创建接口统一把空父资源及资源字段归类为
  `GRANIT_ERROR_INVALID_HANDLE`，并保持失败输出为零。
- Texture View、Shader、Upload Batch、Recorder 批量提交和 Pipeline Cache 操作补齐相同的
  无效句柄语义，保留空批次等参数形状错误为 `GRANIT_ERROR_INVALID_ARGUMENT`。
- C++ RAII 包装在底层句柄或父资源已失效时，`reset()` 返回 `INVALID_HANDLE` 的同时清空本地
  状态，避免对象继续表现为有效或在析构时重复销毁。

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
