<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-23 S-50 Model Viewer 教程迁移验收

## 范围

本记录保存 S-50 收尾时执行的本地验证。当前学习路径以
[教程目录](../tutorials/README.md)为准，迁移目标与保留边界见
[S-50 计划](../plans/S-50-0.29.0-model-viewer-tutorial-migration.md)。

迁移新增 09 Model Loading 和 10 Model Viewer，把原 Model Viewer 的 Application Core、桌面与
Web 壳层、离屏验收和测试移入最终教程，并把两章共用的 GPU Scene 留在
`examples/common/model_viewer`。旧 `examples/samples/model_viewer` 不再保留。

## Windows

变基到包含 0.28.1 坐标修复的最新 `main` 后，执行以下完整矩阵：

- Windows Clang Debug shared：102/102 通过；
- Windows MSVC Debug shared：96/96 通过；
- Windows MSVC Debug static：91/91 通过。

验证范围包含 01～10 教程、GPU Smoke、文档链接、C/C++ Header、公共 DLL 导出快照、构建树
Consumer 和静态链接。新增的 Mesh `bind` / `draw` 已进入 0.29.0 RenderPipeline ABI 快照。

## Emscripten 与浏览器

Emscripten Debug 完整构建和 CTest 13/13 通过。Chrome Headless WebGPU 验证 Tutorial 01、08 与
10；其中 Model Viewer 使用仓库内 `model_viewer_fixture.gltf` 覆盖多帧提交、质量与光照切换、输入、
Resize、Fetch、异步 Pipeline、上传取消、失败诊断和资源释放。

## 验收结论与未覆盖项

Tutorial 09/10、共享 GPU Scene、桌面/Web/离屏入口、工作流和运行指南已完成单一入口切换，仓库
当前内容不存在旧 Sample 路径或旧用户目标引用。

本机没有执行 Linux Clang；该平台仍由合并前手动 Actions 验证。本记录不代表 0.29.0 已发布，
也不替代正式版本发布验收。
