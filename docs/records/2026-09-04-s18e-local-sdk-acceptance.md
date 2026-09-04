<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-04 S-18E 本地 SDK 验收

## 结论

0.5.0 开发分支已通过 Windows Clang 共享与静态 SDK、本地安装 Consumer，以及 Emscripten
Debug/Release 浏览器 WebGPU 验收。Linux 和 Gneiss 独立分支升级仍待后续验证，因此本记录不代表
S-18E 已全部完成。

## Windows

- `windows-clang-debug` 构建通过，64 项测试全部通过。
- `windows-clang-static-debug` 构建通过，54 项测试全部通过。
- 共享与静态安装包的兼容版本、精确版本、错误主版本和未知 component 选包检查通过。
- 共享与静态安装包各自运行 7 个 C11/C++20 Consumer，全部通过。
- Window Consumer 已从安装 SDK 创建窗口并查询逻辑尺寸、Framebuffer 尺寸与内容缩放。

## Emscripten WebGPU

- 使用锁定的 Emscripten 5.0.6 完成 `emscripten-debug` 与 `emscripten-release` 构建。
- Chrome 无头测试验证多帧渲染、质量切换、输入、Resize、资产 Fetch 与资源释放。
- 外部 glTF Buffer 缺失时的诊断路径验证通过。

## 待完成

- 手动运行 Linux Actions，覆盖 Clang/GCC、共享/静态以及 XCB/Wayland 路径。
- 在不混入 Gneiss 现有 `feat/ver022-asset-hot-reload` 分支的前提下建立独立 0.5.0 接入分支，
  使用窗口状态查询初始化尺寸与缩放，并完成 Package/Fetch 验收。

当前实施范围与剩余工作以
[S-18 计划](../plans/S-18-0.5.0-platform-upstream-integration.md)为准。
