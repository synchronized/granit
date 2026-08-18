<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-08-18 Windows/Linux CI 验证记录

## 目的

记录 Actions 恢复后对 S-03、S-04、S-07 与 S-07E 的跨平台验收结果。当前行为和后续顺序仍以
对应 Reference、Plan 与[路线图](../roadmap.md)为准。

## 验证范围

| 工作流 | 编译器 | 链接模式 | 主要覆盖 |
|---|---|---|---|
| Windows | MSVC | 共享、静态 | 构建、测试、安装导出、C/C++ Consumer |
| Linux | Clang、GCC | 共享、静态 | 构建、测试、XCB/Wayland、安装导出与 Consumer |

Linux Runner 使用 Mesa 软件 Vulkan、Xvfb 和 Weston Headless，覆盖 XCB 与 Wayland Surface、
Swapchain、Window、Input 及相关示例。安装阶段验证核心、RenderPipeline、Window 和 Input 的
C/C++ Consumer，并检查安装导出不泄漏源码树、测试依赖或 Vulkan 私有依赖。

## 结果

- 提交 `2603ae9` 的 Windows 运行 `32109589768` 与 Linux 运行 `32109589772` 均成功。
- 提交 `bb91f6a` 的 Windows 运行 `32110283402` 与 Linux 运行 `32110283403` 均成功。
- 第二轮继续覆盖 Input 内部对象库重构，确认共享与静态链接模式均未依赖内部导出符号。
- S-03、S-04、S-07 与 S-07E 的远端复测阻塞解除，当前矩阵验收完成。

## 未覆盖范围

Linux workflow 未启用可选 SDL3/ImGui Integration，因此本记录不证明 S-08 的 Linux XCB/Wayland
运行路径已经通过。该项仍保留在路线图近期执行顺序中。
