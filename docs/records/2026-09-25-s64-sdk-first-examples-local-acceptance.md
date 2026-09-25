<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-25 S-64 SDK-first 示例本地验收

## 验收范围

本次验收覆盖安装后 SDK Window Quickstart、shared SDK Consumer 集成、Example Asset System 契约，
以及 Tutorials、Samples 和独立示例的 C++ 包装使用面。没有新增公共 API、ABI、资产格式或发布附件。

## 已完成结果

- 新增独立 `examples/standalone/window_clear` CMake Consumer，只通过安装后的 `granit::window` 创建
  Window、Renderer、Surface、Swapchain 和 Frame Context，并完成清屏与 Present。
- 安装包检查会配置并构建独立 Quickstart；Windows CI 直接运行，Linux CI 在虚拟显示中运行。
- Example Asset System 形成集中契约：业务 Key 只含 Mount 与逻辑路径，Desktop/Web Source、部署和
  glTF 相对 URI 保持在示例私有层。
- Model Viewer 场景值与采样配置使用 C++ 强类型；C 句柄只留在显式 C ABI 验收与句柄身份测试中。

## 本地验证

- Windows Clang shared Debug：完整构建通过，CTest 92/92 通过。
- Windows Clang static Debug：完整构建通过，CTest 86/86 通过。
- Emscripten Debug：完整构建通过，CTest 14/14 通过。
- Chrome WebGPU：正式 Model Viewer 与运行时验收页面通过；覆盖多帧渲染、质量与光照切换、输入、
  Resize、资产 Fetch、异步 Pipeline、上传取消、缺失 Buffer 诊断和资源释放。
- 当前 shared SDK 安装包检查通过，独立 Quickstart 构建并完成三帧 Smoke。
- Documentation、clang-format 和 `git diff --check` 通过。

## 待完成

功能分支仍需通过 Linux、Windows、Emscripten、Quick Check 与 Documentation 远端工作流。合入
`main` 后按发布流程从最新 `main` 创建 `release/0.31.0`，再修改版本文件并发布。
