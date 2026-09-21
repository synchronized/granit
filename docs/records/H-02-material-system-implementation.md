<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-02：材质系统实施记录

## 结果

H-02 完成了材质模板、实例参数、Texture/Sampler 绑定、Shader 变体、Pipeline 缓存、热替换与错误
材质回退的首轮闭环，并验证了确定性材质包、反射数据和性能边界。这些结果随后演进为当前公共
Material、Shader Library 与 AssetTools 契约。

当前 API、资产格式和绑定布局已经经过多个后续版本调整，不应从早期 H-02 设计推断。使用者应以
[Material Reference](../reference/material.md)、[Shader Library](../reference/shader-library.md)和
[AssetTools](../reference/asset-tools.md)为准。

## 已验证边界

- Material 负责参数、资源绑定、静态变体和 Pipeline 选择，不接管 Scene 或资产数据库。
- 实例更新采用候选状态，校验和 GPU 资源创建成功后才替换当前状态。
- 离线包使用确定性编码和内容摘要，损坏输入在创建 GPU 状态前被拒绝。
- Pipeline 缓存键包含渲染状态、布局、Shader 和目标格式，不依赖对象地址。
- 普通使用者可以使用 Material 门面，也可以绕过它直接调用核心 Renderer。

## 历史与当前入口

- [H-02 计划](../plans/H-02-material-system.md)保存目标和完成差异。
- [材质包格式计划](../plans/H-02-material-package-format.md)保存早期持久化格式决策。
- [完整历史快照](history/H-02-material-system-history.md)保存原始设计和逐阶段实施日志，仅用于追溯。

后续工作应更新当前 Reference 或建立新计划，不再向历史快照追加内容。
