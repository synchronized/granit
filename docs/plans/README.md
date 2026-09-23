<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 开发计划文档

本目录保存单项路线图任务的目标、设计、步骤和验收条件。计划不代表已经实现的公共能力；当前
行为以 Reference、Concept 和仓库实现为准。全局优先级见[路线图](../roadmap.md)，执行结果见
[实施记录](../records/README.md)。

## 当前计划

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
