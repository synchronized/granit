<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-24 S-60 Model Viewer 共享渲染运行时本地验收

## 结果

Desktop/Web 已共用同步 `render_runtime` 管理 Renderer、Surface、Swapchain、Render Pipeline、Canvas、
字体、Scene 上传、质量切换、帧执行、统计和有序释放。Desktop 的剩余调度门面已改名为
`render_thread`，Web 平台入口不再直接创建、绘制或销毁上述 GPU 对象。

## 已通过

| 检查 | 结果 |
| --- | --- |
| Windows Clang Model Viewer 构建 | 通过 |
| Windows Vulkan `--smoke-test --no-ui` | 通过 |
| `granit.sample.model_viewer_support` | 通过 |
| `granit.sample.model_viewer_imgui` | 通过 |
| `granit.sample.model_viewer_desktop_shell` | 通过 |
| Emscripten Release Model Viewer 构建 | 通过 |
| Chrome WebGPU Model Viewer 完整行为验收 | 通过 |
| 文档检查 | 通过 |
| `git diff --check` | 通过 |

Chrome 验收覆盖多帧渲染、质量与光照切换、输入、Resize、资产 Fetch、资源释放、原生异步 Pipeline
创建、上传取消与回滚，以及外部 Buffer 缺失诊断。关闭时在 Renderer 销毁前读取统一运行时的资源
统计，确认活资源归零。

## 平台边界

Desktop 仍使用 threaded executor 和专用渲染线程，Web 仍使用 inline executor、Fetch 与 Asyncify。
浏览器 Pipeline 预热和自动化导出保留在 Web 壳层；其余 GPU 生命周期由同一具体运行时实现，不使用
平台继承层，也不进入 Granit 安装 SDK。
