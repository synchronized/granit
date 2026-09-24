<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-24 S-59 Model Viewer 渲染任务运行时本地验收

## 结果

S-59 的拥有型渲染任务执行器、Desktop/Web 执行策略和共享 `model_viewer_runtime` 已完成本地实现。
Windows、Emscripten 编译、共享单元测试、Chrome WebGPU 行为验收及文档检查通过。

## 已通过

| 检查 | 结果 |
| --- | --- |
| Windows Clang Model Viewer 支持测试与 Desktop Sample 构建 | 通过 |
| `granit.sample.model_viewer_support` | 通过 |
| `granit.sample.model_viewer_imgui` | 通过 |
| `granit.sample.model_viewer_desktop_shell` | 通过 |
| Emscripten Debug `granit_sample_model_viewer_web` 构建 | 通过 |
| Emscripten Release `granit_sample_model_viewer_web` 构建 | 通过 |
| Chrome WebGPU Model Viewer 完整行为验收 | 通过 |
| 文档检查，280 个 Markdown 文件 | 通过 |
| `git diff --check` | 通过 |

共享测试覆盖 inline/threaded 执行、帧替换、不可丢弃任务、完成回执、同步等待、重复停止和 Runtime
Renderer 阶段。Desktop/Web 均已使用 Runtime 连接资产加载与 Viewer Core。

## 浏览器验收修复

初次验收在异步 Pipeline 预热阶段停在 `GRANIT_PROGRESS:pipelines:0:4`。当前浏览器与 Emdawn 组合
中，启动回调执行过前序 Asyncify 上传后，不一定再次调度主循环 tick，导致 WaitAnyOnly Pipeline
Future 无法继续交付。

Web 启动阶段现在通过 `emscripten_sleep(0)` 主动让出浏览器事件循环，并在 30 秒边界内推进 Renderer
事件与 Pipeline Future。完整验收确认多帧渲染、质量与光照切换、输入、Resize、资产 Fetch、资源
释放、异步 Pipeline 创建、上传取消与回滚均通过。

验收还发现共享加载会话曾把外部 Buffer 读取错误退化为通用 `asset-fetch`。加载会话现在保留结构化
错误类别，Web 恢复报告 `asset-resource-fetch`；单元测试与浏览器缺失 Buffer 用例均覆盖该行为。
