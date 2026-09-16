<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 开发计划文档

本目录保存单项路线图任务的目标、设计、步骤和验收条件。计划不代表已经实现的公共能力；当前
行为以 Reference、Concept 和仓库实现为准。全局优先级见[路线图](../roadmap.md)，执行结果见
[实施记录](../records/README.md)。

## 最近完成

- [S-45：0.26.0 文档一致性与历史收敛](S-45-0.26.0-documentation-convergence.md)——修正当前
  事实漂移，收敛文档职责与自动检查。

完整的已完成计划见[完成计划索引](completed.md)。计划文件仍保留在本目录中，便于追溯设计目标、
验收条件和最终差异；当前行为应以 Reference、Concept 和仓库实现为准。

## 待开始

- [S-46：API 教程与可运行示例阶梯](S-46-api-tutorial-example-ladder.md)——已完成路径设计和
  `minimal_renderer`、`triangle` 及 `01`～`06` 教程，正在推进基础教程验收与维护收敛。

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
