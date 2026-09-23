<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-23 S-54 特性教程与 Example Application 本地验收

## 验收范围

本次验收覆盖仓库私有 Example Application、两个编号特性教程、Model Viewer 的 Sample
迁移，以及桌面和浏览器构建入口。公共 API、ABI 和 SDK 安装内容没有变化。

## 已完成结果

- 新增 `examples/common/application`，统一 Window、Renderer、Surface、Swapchain、事件、Resize、
  Acquire、Present、取消与恢复；内容层仍自行选择 Frame Context 或 Render Pipeline。
- 将原 01～05 与 ImGui 内容收敛为 `01_cube`，以确定性木箱纹理展示低层 Renderer、Mesh、Depth、
  Camera、Frame Context 和 Canvas。
- 将 Material、Lighting 与 Render Pipeline 内容收敛为 `02_pbr_assets`，以带完整 PBR 顶点属性的
  Sphere 展示 Material、Scene、Shadow、HDR、后处理和空场景首帧。
- 将 Model Viewer 移入 `examples/samples` 并消除重复目录层级；删除已由 Viewer 与支持代码测试
  覆盖的最小 Model Loading 应用，同步更新测试、浏览器产物、工作流和文档入口。
- Example Application 保持仓库私有，不进入安装导出，也不向教程传播 ImGui、glTF 或 Render
  Pipeline 依赖。

## 本地验证

- Windows Clang Debug：目录收敛后完整 `ctest` 共 92 项全部通过；两个教程以及 Model Viewer 的
  桌面、Core、ImGui、平台壳与离屏目标构建通过。
- Emscripten Debug：`01_cube`、`02_pbr_assets` 和 Model Viewer Web 目标构建通过。
- Chrome WebGPU：两个教程和 Model Viewer 的多帧、输入、Resize、资产加载与资源释放行为测试通过。
- `actionlint`、Documentation 链接检查和 `git diff --check` 通过。
- Windows Debug 安装结果中不存在 `granit_example_application`，确认私有示例框架未进入 SDK。

## 待远端验证

Linux Vulkan 与 Emscripten Release 仍由 Pull Request Actions 验证。远端矩阵通过后，S-54 可从当前
计划移入完成计划索引。
