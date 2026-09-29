<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-29 S-79 ImGui Integration 教程归位验收

S-79 已删除职责重叠的 ImGui 综合 Sample，并将外部 SDL Window 接入改为 `08_sdl_imgui` 教程。
本记录保存阶段结果；当前设计以
[S-79 计划](../plans/S-79-0.42.0-imgui-integration-layout.md)和源码为准。

## 实施结果

- `examples/samples/imgui` 已删除，Model Viewer 成为 `samples` 中唯一完整应用；
- `08_sdl_imgui` 聚焦 SDL Window、事件循环、Surface、ImGui Draw Data、Canvas 和 Present；
- Desktop 与 Web 共用界面内容、字体及自定义纹理映射，分别适配阻塞和浏览器协作式循环；
- CSV Profile、GPU Timestamp 和 ImGui Demo Window 已从教程移除，性能入口继续由 Benchmark 承担；
- Vulkan 固定画面移入 `tests/integrations/imgui`，浏览器验收移入 `tests/web/imgui`；
- CMake、Linux/Emscripten 工作流、教程索引、示例指南和 Integration Reference 已使用新目标名称。

## 验证结果

| 环境 | 验证 | 结果 |
|---|---|---|
| Windows Clang Debug | 完整构建与 CTest | 105/105 通过 |
| Windows Vulkan | Tutorial Smoke 与 ImGui 1×/2× DPI 固定画面 | 通过 |
| Emscripten Release | `granit_tutorial_08_sdl_imgui` 编译与链接 | 通过 |
| Chrome WebGPU | 1×/2× DPI、纹理、裁剪、点击、输入、Resize 与 Shutdown | 通过 |
| 仓库契约 | 文档链接、CI Workflow 契约、Actionlint 与 `git diff --check` | 通过 |

Linux X11 与 Wayland 命令已经随新目标更新，需由远端 Linux Workflow 完成平台验收。本阶段没有修改
公共 API、版本号或 Release。
