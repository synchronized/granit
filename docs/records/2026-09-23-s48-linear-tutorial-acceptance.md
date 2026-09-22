<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-23 S-48 线性教程本地验收

## 范围

本记录保存 S-48 收尾时已经执行的本地验证。当前教程行为以
[教程目录](../tutorials/README.md)为准，设计目标与保留边界见
[S-48 计划](../plans/S-48-0.28.0-linear-tutorial-series.md)。

验收覆盖 01 Window 至 08 ImGui、Model Viewer 强类型帧提交链、构建树 C++ consumer，以及桌面
Vulkan 和浏览器 WebGPU 路径。

## Windows Clang

环境为 Windows Clang Debug shared preset，使用锁定 DXC 与 Tint 工具链：

```powershell
cmake --build --preset windows-clang-debug
ctest --preset windows-clang-debug --output-on-failure
```

结果为 101/101 通过。范围包含：

- 01～08 共八个 Tutorial Smoke；
- Renderer、Render Pipeline、Window、GPU 离屏和窗口 Smoke；
- C/C++ Header、ABI 导出、生命周期与安装构建树 consumer；
- AssetTools Shader、Material、Texture、Environment；
- 文档链接与版本检查。

## Emscripten 与浏览器

```powershell
cmake --build --preset emscripten-debug
ctest --preset emscripten-debug --output-on-failure
```

结果为 13/13 通过。Chrome Headless WebGPU 另外验证：

- Tutorial 01 多帧推进和 Canvas Resize；
- Tutorial 08 Canvas Item、统一指针输入、Swapchain Resize 和持续渲染；
- Model Viewer 多帧、质量与光照切换、输入、Resize、资产 Fetch、异步 Pipeline、上传取消与
  资源释放。

## 验收结论与未覆盖项

S-48 的八章源码、文档、C++ 强类型使用面与本地自动验证形成闭环，可以进入合并前平台矩阵。

本机没有重复执行 Linux Clang、Windows MSVC 和 Windows static preset；这些平台由合并前手动
Actions 与发布候选矩阵验证。本记录不代表 0.29.0 已发布，也不替代正式版本发布验收。
