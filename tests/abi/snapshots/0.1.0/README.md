<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Granit 0.1.0 C ABI 快照

这是 Core、RenderPipeline、Window 与 Input 的首份正式 ABI 快照。它用于检测变更，不代表 0.x
已承诺稳定
ABI；有意的破坏性变更仍按[版本与兼容策略](../../../../docs/reference/compatibility.md)处理。

## 身份与范围

| 项目 | 快照身份 |
|---|---|
| Granit 版本 | `0.1.0` |
| component | Core、RenderPipeline、Window、Input |
| 平台 | Windows、Linux |
| 架构 | x86_64 |
| 编译器 | MSVC、Clang、GCC |
| 接口 | 各 component 导出宏标记的公共 C ABI |

`platform_identity.h` 在编译期校验版本、平台、架构和编译器身份。`core_identity.h` 与
`optional_components_identity.h` 标识 component；`../../export_symbols_test.cpp` 动态比较各共享库
的完整公共 C 符号集合，`../../layout_h.c` 和 `../../../headers/pipeline_*_h.c`、`window_h.c`、
`input_h.c` 比较公共类型大小、字段偏移、对齐、枚举和标志位。

第三方 Integration 仍是实验性 C++ component，不属于 C ABI 快照。各 component 的快照互相独立；
Core 稳定不自动表示其他 component 稳定。

## 更新规则

只有经过审查的公共 ABI 变更才可更新基线。更新时必须同时修改版本身份、符号或布局差异、兼容
策略说明和迁移信息；不得仅为通过 CI 而覆盖快照。
