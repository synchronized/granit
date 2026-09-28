<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-28 S-72 Asset Manager 与任务系统验收

S-72 已在 `examples/common` 内建立示例私有 Task System 和单一 Asset Manager，并完成现有调用方迁移。
本记录保存阶段结果；当前设计以
[S-72 计划](../plans/S-72-0.39.0-asset-manager-and-task-system.md)和源码为准。

## 实施结果

- Application Host 统一拥有 Task System 与 Asset Manager，并在每个 Tick 发布主线程完成任务；
- Cube、PBR Assets 与 Model Viewer 只通过强类型 `load<T>()` 取得业务资产；
- Desktop 文件、Web bundled MEMFS 和 Web external Fetch 通过平台注册入口组合；
- Blob、图片与 glTF Loader 通过注册接入，外部 Buffer 和图片依赖进入同一请求图；
- 单资源句柄、Group 进度、请求合并、缓存、取消和确定性错误均有测试覆盖；
- 旧 `asset_system`、`asset_batch`、`asset_request` 和 `document_loader` 已删除；
- Model Viewer 浏览器验收按 CPU 资产发布、GPU 规划、上传和 Pipeline 阶段观察进度。

## 验证结果

| 环境 | 验证 | 结果 |
|---|---|---|
| Windows Clang Debug | 完整构建与 `ctest --preset windows-clang-debug` | 100/100 通过 |
| Emscripten Debug | 完整构建与 `ctest --preset emscripten-debug` | 16/16 通过 |
| Chrome WebGPU | Tutorial 01、Tutorial 02 与正式 Model Viewer | 通过 |
| Chrome WebGPU | Model Viewer 多帧渲染、资产依赖、取消、回滚和缺失 Buffer 诊断 | 通过 |

Emscripten preset 禁用异常捕获，因此只在支持异常的宿主构建验证“任务异常转结果”；Emscripten 仍覆盖
任务排队、完成发布、资产加载和浏览器运行语义。浏览器控制台中的 favicon 404 不属于 Granit 资产
请求，不影响验收结果。

## 边界

本阶段没有新增公共 C ABI 或安装头。Asset Manager、Task System、Source 和 Loader 仍是示例基础设施，
后续只有出现第二个仓库外使用者并确认稳定契约后，才考虑提升为公共 SDK。
