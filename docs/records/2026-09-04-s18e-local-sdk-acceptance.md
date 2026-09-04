<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-04 S-18E SDK 验收

## 结论

0.5.0 开发分支已通过 Windows Clang 共享与静态 SDK、本地安装 Consumer、Emscripten
Debug/Release 浏览器 WebGPU，以及 Linux Clang/GCC 共享与静态 SDK 验收。Granit 侧 S-18E 已完成；
Gneiss 的依赖升级与接入验证由其项目独立执行，不作为 Granit 的完成门槛。

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

## Linux

- 手动运行 [Linux Actions 33847579149](https://github.com/synchronized/granit/actions/runs/33847579149)，
  六个 Job 全部通过。
- Clang/GCC 的共享与静态构建、测试及安装 Consumer 全部通过。
- SDL3 与 ImGui 的 X11、Wayland 共享/静态运行时验证全部通过。

## Consumer 后续

- Gneiss 可在自身分支升级到 Granit 0.5.0，并使用窗口状态查询初始化逻辑尺寸、Framebuffer 尺寸
  与内容缩放。
- Granit 不修改或接管 Gneiss 的依赖分支、场景、资产与渲染服务实现。

当前实施范围与剩余工作以
[S-18 计划](../plans/S-18-0.5.0-platform-upstream-integration.md)为准。
