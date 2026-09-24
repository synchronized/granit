<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-24 S-62 Model Viewer 统一 Render Service 本地验收

## 结果

Desktop/Web 已通过同一个 `render_service` 使用 `render_runtime`。执行位置由
`render_task_executor` 策略决定：Desktop 的 `threaded_render_service` 保留专用线程、可替换帧、
异步上传和完成回执，Web 使用浏览器主线程 inline executor。Web 应用层不再直接持有或调用
`render_runtime`。

## 已通过

| 检查 | 结果 |
| --- | --- |
| Windows Clang Model Viewer 与 Support Test 构建 | 通过 |
| Model Viewer Support、ImGui 与 Desktop Shell CTest | 通过 |
| Windows Vulkan Model Viewer `--smoke-test` | 通过 |
| Emscripten Release 正式、测试与 Platform Smoke 构建 | 通过 |
| Chrome 正式 Model Viewer 页面与测试接口隔离 | 通过 |
| Chrome 测试 Model Viewer 完整行为验收 | 通过 |
| Chrome WebGPU Platform Smoke | 通过 |
| `git diff --check` | 通过 |

统一执行器契约测试同时覆盖 inline 与 threaded 策略的初始化、同步帧提交、结果回传、运行状态和
停止行为。浏览器验收继续覆盖多帧渲染、质量与光照切换、输入、Resize、资产 Fetch、资源释放、
异步 Pipeline、上传取消与回滚，以及外部 Buffer 缺失诊断。

## 剩余验证

远端 Linux 和完整 Release 矩阵尚未运行。本阶段只改动 Sample 私有实现，不改变 Granit 公共 API。
