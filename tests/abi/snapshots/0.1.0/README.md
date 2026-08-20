<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Granit 0.1.0 核心 C ABI 快照

这是核心 `granit` component 的首份正式 ABI 快照。它用于检测变更，不代表 0.x 已承诺稳定
ABI；有意的破坏性变更仍按[版本与兼容策略](../../../../docs/reference/compatibility.md)处理。

## 身份与范围

| 项目 | 快照身份 |
|---|---|
| Granit 版本 | `0.1.0` |
| component | `Core`（共享库目标 `granit`） |
| 平台 | Windows、Linux |
| 架构 | x86_64 |
| 编译器 | MSVC、Clang、GCC |
| 接口 | `GRANIT_API` 导出的 C ABI |

`core_identity.h` 在编译期校验上述身份。`../../export_symbols_test.cpp` 保存并动态比较完整公共
C 符号集合，`../../layout_h.c` 保存并比较 C 类型大小、字段偏移、对齐、枚举和标志位。

RenderPipeline、Window、Input 和第三方 Integration 是独立 component，不属于这份核心快照；
它们应在各自完成稳定性审计后建立独立快照。

## 更新规则

只有经过审查的公共 ABI 变更才可更新基线。更新时必须同时修改版本身份、符号或布局差异、兼容
策略说明和迁移信息；不得仅为通过 CI 而覆盖快照。
