<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-15 S-44F 0.25.0 本地候选包验收

本记录只保存本地执行结果。0.25.0 Window/Input/呈现的当前接口见
[Window](../reference/window.md)、[Surface](../reference/surface.md) 和
[迁移指南](../guides/migrate-0.24-to-0.25.md)。

## 环境与结果

- Windows Clang + Ninja Debug：共享库完整构建通过，CTest 91/91；静态库完整构建通过，
  CTest 81/81。两种模式均覆盖 GPU 离屏、Window Render Pipeline、Win32 Surface、
  ABI 导出与公共头测试。
- 共享与静态包分别安装到新的前缀；安装导出审计、CMake 完整选包矩阵通过。矩阵包括
  0.24 版本请求、旧 `Input` component 和未知 component 的拒绝检查。
- 两种安装包的 C11/C++20 Consumer 各 7/7 通过，覆盖 Core、RenderPipeline、Window 和
  Window 输入入口。
- Emscripten Debug 完整构建与 Node.js CTest 3/3 通过；安装导出审计、安装后的 Web
  C/C++ Consumer 3/3 通过。Chrome 无头 WebGPU 平台 Smoke、上传回滚与错误诊断通过；
  浏览器 ImGui 1×/2× DPI 及多帧渲染验收通过。

## 仍需跨平台验证

本机没有 Linux/WSL 发行版，无法直接运行 Linux 共享/静态安装包及 XCB/Wayland 真实 Surface
闭环。发布前须在同一提交上运行 Linux 和 Windows 的远端矩阵，确认安装 Consumer 与平台
Surface 测试全部通过；本地结果不代替这一项。
