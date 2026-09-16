<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-16 S-45 文档一致性基线

## 结果

S-45 已完成当前事实修正、Concept/Reference 职责收敛、导航压缩、长记录历史分离和确定性自动
检查。变更只涉及文档、公共头注释、项目描述元数据与文档检查脚本，不改变公共 API、ABI、资产
格式或运行时行为。

## 收敛结果

- README 的最新版本、Release 入口和模块表已与 0.25.0 一致。
- Changelog 已覆盖仓库全部 `v0.*` 发布标签，并补齐 0.17.0。
- 当前 Guide 与 Concept 不再把 Input 描述为独立 component。
- Vulkan Concept 已覆盖当前 HAL、资源、提交、回收和 Win32/XCB/Wayland 呈现路径。
- 资源总览只定义共享值类型，Texture、Sampler 和 Render Target 分别成为能力权威来源。
- Roadmap 从 535 行压缩为当前能力、正在实施、暂缓候选和长期方向。
- H-02、H-03、H-05、H-07 原入口收敛为约 30 行摘要，完整逐阶段内容保存在
  `docs/records/history/`。

## 自动检查

`cmake/check_documentation.cmake` 现在检查：

- 245 个自有 Markdown 文件的相对链接；
- Guide、Reference、Concept、Plan 与 Record 的分类索引；
- 根 README 180 行上限、CMake 使用入口和教程验证命令；
- CMake 项目版本、README 最新版本、Release 链接和当前 Changelog 标题一致；
- Git 仓库可用时，每个发布标签都存在 Changelog 版本标题；
- 当前文档不再引用已删除的独立 Input component 契约；
- 已发布的 S-42 不再保持“等待远端验收”状态。

本地执行 `cmake -DGRANIT_SOURCE_DIR="$PWD" -P cmake/check_documentation.cmake` 与
`git diff --check` 均通过。
