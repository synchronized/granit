<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 开发计划文档

本目录保存单项路线图任务的目标、设计、步骤和验收条件。计划不代表已经实现的公共能力；当前
行为以 Reference、Concept 和仓库实现为准。全局优先级见[路线图](../roadmap.md)，执行结果见
[实施记录](../records/README.md)。

## 当前计划

- [S-62：Model Viewer 统一 Render Service](S-62-model-viewer-render-service.md)——统一 Desktop/Web
  的渲染调用门面，并保留 threaded/inline 执行策略。
- [S-61：Model Viewer 共享 ImGui 与浏览器测试边界](S-61-model-viewer-shared-imgui-browser-tests.md)
  ——统一 Desktop/Web Viewer 面板，并分离正式浏览器产物与测试探针。
- [S-60：Model Viewer 共享渲染运行时](S-60-model-viewer-shared-render-runtime.md)——提取同步 GPU
  运行时，由 Desktop/Web 通过 threaded/inline executor 复用同一资源和帧执行逻辑。
- [S-59：Model Viewer 渲染任务运行时](S-59-model-viewer-render-task-runtime.md)——以强类型任务统一
  Desktop/Web 的 GPU 执行语义，并提取跨平台 Viewer 生命周期。
- [S-58：统一 Example Asset System](S-58-unified-example-asset-system.md)——以逻辑 Mount、统一请求和
  内部平台 Source 取代 Store/Loader 并列入口，并统一 Tutorial、glTF 与 Model Viewer 加载编排。
- [S-57：Model Viewer 运行时架构](S-57-model-viewer-runtime-architecture.md)——保持现有目录和
  Desktop/Web 执行语义，拆分平台组合入口、渲染执行、加载编排与浏览器验收边界。
- [S-56：Window Target 与 SDL3 后端](S-56-window-target-and-sdl3-backend.md)——分离 Window
  Backend 与绑定目标，支持原生 Emscripten 多 Canvas，并通过可选 SDL3 后端提供统一 Window API；
  本地实施与浏览器、安装 Consumer 验证已完成，等待远端 Linux 矩阵。
- [S-55：Example Common 架构整理](S-55-example-common-architecture.md)——保持现有短目录入口，
  统一 SDL target，整理资产与 Web 资源边界，并专项分析 Model Viewer。
- [S-54：0.30.0 特性教程与 Example Application](S-54-0.30.0-feature-tutorial-framework.md)——抽取
  跨平台示例应用壳，将教程压缩为 Cube 与 PBR Assets，将完整 Model Viewer 移回 Samples，并删除
  重复的最小加载应用；本地实施与浏览器验证已完成，等待远端矩阵。
- [S-53：0.30.0 文档、教程与发布面收敛](S-53-0.30.0-documentation-tutorial-release.md)——建立
  Frame 与命令录制分层 Concept，整理教程学习路径，并把正式 Release 收敛为 shared SDK；本地
  实施与浏览器验证已完成，等待远端矩阵。

## 最近完成

- [S-52：0.29.0 版本收口与发布验收](S-52-0.29.0-release-acceptance.md)——完成迁移说明、
  跨平台矩阵、四套 SDK 候选包、校验和与正式发布复验。
- [S-51：0.29.0 Shader Library 逻辑名称](S-51-0.29.0-shader-library-logical-names.md)——把
  清单逻辑名称写入 `.grshlib`，统一运行时与 Material 工具入口，并删除独立 Shader 索引和生成 ID。
- [S-50：0.29.0 Model Viewer 教程迁移](S-50-0.29.0-model-viewer-tutorial-migration.md)——新增
  09 Model Loading，将跨后端 Model Viewer 收敛为 Tutorial 10，并删除重复 Sample。
- [S-48：线性入门教程与配套示例](S-48-0.28.0-linear-tutorial-series.md)——完成从 Window 到
  ImGui 的八章连续教程、强类型 C++ 使用面审计和桌面/浏览器自动验证。
- [S-49：0.28.0 Window 跨平台可选主循环](S-49-0.28.0-window-loop.md)——以可选托管 Loop 统一
  桌面与 Emscripten 的逐帧调度，同时保留引擎自有事件循环。
- [S-47：Window 跨平台入口收敛](S-47-window-platform-convergence.md)——让 Emscripten 与
  Win32、XCB、Wayland 共享 Window、输入和 Surface 公共流程。
- [S-45：0.26.0 文档一致性与历史收敛](S-45-0.26.0-documentation-convergence.md)——修正当前
  事实漂移，收敛文档职责与自动检查。

完整的已完成计划见[完成计划索引](completed.md)。计划文件仍保留在本目录中，便于追溯设计目标、
验收条件和最终差异；当前行为应以 Reference、Concept 和仓库实现为准。

## 已取代

- [S-46：API 教程与可运行示例阶梯](S-46-api-tutorial-example-ladder.md)——其主题式教程方案由
  S-48 的线性可见结果教程取代；已经完成的最小 Renderer 和 Triangle 实现继续复用。

## 暂缓与重新评估

- [D-09：Bindless Resource Table](D-09-bindless-resource-table.md)——等待真实绑定压力证据。
- [H-09B：透明 PBR 正确性](H-09B-transparent-pbr-correctness.md)——等待明确产品场景和正确性需求。

## 状态与维护

- **草案**：仍有影响方向的未决问题。
- **已确认**：主要设计已经同意，可以进入实施。
- **实施中**：代码或文档正在落地。
- **已完成**：验收通过并记录最终差异。
- **已暂停**：存在明确阻塞或重新评估条件。

计划完成后保留目标与最终差异，详细日志转入 Record，当前行为同步到 Reference 或 Concept。文档
结构和生命周期统一遵循[项目文档规范](../../DOCUMENTATION_GUIDE.md)。
