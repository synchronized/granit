<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-24 S-59 Model Viewer 渲染任务运行时本地验收

## 结果

S-59 的拥有型渲染任务执行器、Desktop/Web 执行策略和共享 `model_viewer_runtime` 已完成本地实现。
Windows、Emscripten 编译、共享单元测试及文档检查通过；本机 Chrome 的正式 Model Viewer 验收停在
既有异步 Pipeline 预热阶段，未将浏览器验收标为通过。

## 已通过

| 检查 | 结果 |
| --- | --- |
| Windows Clang Model Viewer 支持测试与 Desktop Sample 构建 | 通过 |
| `granit.sample.model_viewer_support` | 通过 |
| `granit.sample.model_viewer_imgui` | 通过 |
| `granit.sample.model_viewer_desktop_shell` | 通过 |
| Emscripten Debug `granit_sample_model_viewer_web` 构建 | 通过 |
| Emscripten Release `granit_sample_model_viewer_web` 构建 | 通过 |
| 文档检查，280 个 Markdown 文件 | 通过 |
| `git diff --check` | 通过 |

共享测试覆盖 inline/threaded 执行、帧替换、不可丢弃任务、完成回执、同步等待、重复停止和 Runtime
Renderer 阶段。Desktop/Web 均已使用 Runtime 连接资产加载与 Viewer Core。

## 浏览器待验收项

本机 Chrome 使用 Release 产物和固定 `model_viewer_fixture.gltf` 时，模型文档、CPU 导入和 GPU Scene
上传均完成，随后停在 `GRANIT_PROGRESS:pipelines:0:4`，30 秒后由 Playwright 判定超时。

为排除本次迁移，曾临时将 Web `application.cpp` 恢复到 S-59 开始前的 `877c5fcc` 版本，只适配已
改名的执行器后重新链接并运行同一测试；结果仍停在相同 Pipeline 阶段。旧的 9 月 21 日浏览器产物
可以通过，但它早于当前多项后端和 Sample 改动，不能作为 S-59 前后的同源对照。

因此当前证据不指向 S-59 回归。远端浏览器矩阵需要继续验证；若同样失败，应单独处理 WebGPU 异步
Pipeline 预热，不能通过延长固定 Fixture 超时或跳过检查掩盖问题。
