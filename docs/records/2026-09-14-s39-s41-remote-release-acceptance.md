<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-14 S-39～S-41 远端发布验收

## 结果

S-39～S-41 在提交 `680eb884` 上完成远端验收。Linux、Windows、Emscripten 和 Release
Candidate 均通过；候选包含 Windows/Linux 的共享与静态四套 SDK、安装审计、校验和与 manifest。
本次只生成候选产物，未创建标签或公开 Release。

## 验收期间修复

- PBR 空 pass 的测试输入显式初始化 Light 与 Object 字段，满足 GCC/Clang 的严格聚合初始化检查。
- Model Viewer 浏览器测试只在平台 Smoke 入口检查空帧诊断，避免把入口专属契约施加给其他示例。
- ImGui 浏览器测试等待 Emscripten Runtime 初始化后再调用 Wasm 导出，消除启动阶段的调用竞态。

## 远端验证

- [Linux](https://github.com/synchronized/granit/actions/runs/34811027584)：GCC/Clang、共享/静态、
  Runtime 集成、安装 Consumer 与 Shader Toolchain 全部通过。
- [Windows](https://github.com/synchronized/granit/actions/runs/34811029468)：共享/静态构建、安装审计、
  Consumer 与 Shader Toolchain 全部通过。
- [Emscripten](https://github.com/synchronized/granit/actions/runs/34811031416)：Web 构建与独立 Consumer
  通过，Chrome WebGPU 的平台 Smoke、Model Viewer 和 ImGui 视觉验收全部通过。
- [Release Candidate](https://github.com/synchronized/granit/actions/runs/34811051085)：四套 SDK、安装审计、
  `SHA256SUMS` 和记录 tag、commit、workflow run ID 的 manifest 全部通过。

## 候选范围

- 候选 tag 为 `v0.23.0`，源码提交为
  `680eb8844d6d29ac2aa97d64d0403054f3706f05`。
- 版本范围包含空帧与覆盖层可靠性、Shader Toolchain 配置可靠性和资产目录收敛。
- 正式发布必须继续复用与最终标签提交完全匹配的成功候选，不能把本记录视为公开发布完成。
