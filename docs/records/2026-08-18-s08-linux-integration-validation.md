<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-08-18 S-08 Linux Integration 验证记录

## 目的

记录 SDL3 与 ImGui 可选集成在 Linux X11、Wayland 及共享、静态链接模式下的远端运行验收。

## 环境与范围

- Ubuntu 24.04、Clang Release。
- SDL3 3.4.10 与 ImGui 1.92.9 锁定依赖。
- Mesa 软件 Vulkan 与 Vulkan Validation Layers。
- Xvfb 提供 X11 会话，SDL3 X11 原生值经 X11-xcb 转换后创建 Granit XCB Surface。
- Weston Headless 提供 Wayland compositor，SDL3 直接提供 display 与 surface 原生值。
- 共享库和静态库分别构建、链接并运行。

每个链接模式执行 ImGui Draw Data 转换测试，并在 X11 与 Wayland 下分别运行 SDL3 清屏、
SDL3 + ImGui 三帧 smoke test。

## 结果

- 提交 `2457494` 的 Linux Actions 运行 `32118631643` 成功。
- `integration-runtime (shared)` 与 `integration-runtime (static)` 均成功。
- 主 Linux Clang/GCC × 共享/静态矩阵继续成功，未启用 Integration 的默认构建与安装边界未回退。
- 同一提交的 Windows Actions 运行 `32118631595` 成功。

## 实施差异

首次运行暴露 SDL3 的 Linux 构建依赖缺口：X11 需要 XScreenSaver 开发包，Wayland 视频驱动需要
EGL 开发模块。CI 明确安装相应依赖后，SDL3 配置同时启用 X11 与 Wayland 视频驱动并完成运行验收。

锁定依赖模式只服务源码树构建与验证，不安装 Integration component；安装后的依赖发现仍遵循
[第三方集成参考](../reference/third-party-integrations.md)定义的父项目或 `find_package` 边界。
