<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-13 S-37 远端发布验收

## 结果

S-37 在提交 `84d6556` 上完成全部远端平台与工具链验收。首次运行发现 Linux 对间接包含、名称解析
和聚合初始化的严格检查，以及 Windows 静态安装 Consumer 的配置类型不一致；修复后同一源码状态
通过 Linux、Windows、Emscripten、Documentation、Quick Check、Shader Toolchain Packages 和
Release Candidate。

## 修复

- `src/core/sha256.cpp` 显式包含固定宽度整数类型定义。
- ShaderTools C++ 包装和内部 Library 视图使用跨 GCC、Clang 与 MSVC 一致的名称解析。
- Shader Library 索引和 Material Shader 描述显式初始化全部聚合字段。
- 安装包 Consumer 使用与被测 Release 包一致的配置构建。

## 远端验证

- [Linux](https://github.com/synchronized/granit/actions/runs/34723645148)：GCC/Clang、共享/静态、
  运行时集成和 ShaderTools 全部通过。
- [Windows](https://github.com/synchronized/granit/actions/runs/34723879677)：共享/静态安装、Consumer
  和 ShaderTools 全部通过。
- [Emscripten](https://github.com/synchronized/granit/actions/runs/34723880996)：构建、平台冒烟、
  Model Viewer 和 ImGui 全部通过。Model Viewer 曾遇到 Chrome/Dawn 瞬时 device-lost，同提交独立
  复跑通过。
- [Documentation](https://github.com/synchronized/granit/actions/runs/34723882366) 与
  [Quick Check](https://github.com/synchronized/granit/actions/runs/34723883869) 通过。
- [Shader Toolchain Packages](https://github.com/synchronized/granit/actions/runs/34723885396)：Windows
  与 Linux 工具链包构建及自验证通过。
- [Release Candidate](https://github.com/synchronized/granit/actions/runs/34724283890)：Windows/Linux
  的共享/静态四套 SDK、安装审计、`SHA256SUMS` 和候选 manifest 全部通过；未创建公开 Release。
