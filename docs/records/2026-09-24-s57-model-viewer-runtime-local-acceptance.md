<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-24 S-57 Model Viewer 运行时架构本地验收

## 范围

本轮将 Model Viewer 的跨平台共享数据与平台运行时职责分开：

- Viewer Core 输出纯渲染帧，执行层再组合平台 UI 数据；
- Desktop Renderer、Surface、Swapchain、Pipeline、上传和帧资源由渲染服务集中持有，应用生命周期
  从进程入口移入应用壳；
- Web Pipeline 预热及公共 C API 生命周期验收进入独立对象，JavaScript 导出进入单独编译单元；
- Desktop/Web 重复审计确认 glTF、GPU Scene、Viewer 状态、输入和帧数据已经共享，线程、加载与呈现
  恢复仍保留平台实现。

公共 C/C++ API、安装组件和目录入口未改变。

## 本地结果

| 检查 | 结果 |
|---|---|
| Windows Clang shared Model Viewer Core、ImGui、Desktop Shell 测试 | 3/3 通过 |
| Windows Clang static Model Viewer Core、ImGui、Desktop Shell 测试 | 3/3 通过 |
| Windows Vulkan Desktop Model Viewer，测试 glTF、`--smoke-test --no-ui` | 通过 |
| Emscripten Debug `granit_sample_model_viewer_web` | 构建通过 |
| Chrome WebGPU Model Viewer Fixture | 渲染、质量/光照、输入、Resize、Fetch 与资源释放通过 |
| Chrome WebGPU Pipeline | 原生异步创建通过 |
| Chrome WebGPU 取消、回滚与缺失外部 Buffer | 通过 |
| 文档链接检查 | 通过 |
| `git diff --check` | 通过 |

浏览器验收同时调用两次 Shutdown，并确认 Renderer 活资源和待退休资源均归零。

## 待远端确认

本机没有 Linux XCB/Wayland 环境。合并前还需运行远端 Linux 工作流，确认 Desktop 渲染服务的线程、
Surface 和 SDL3 路径在 Linux Clang 构建与 Smoke 中保持一致。
