<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 测试目录

测试首先按被测组件归属，再用 CTest 标签表达执行环境和成本。根 `CMakeLists.txt` 只负责按依赖顺序
装配子目录；测试目标、源码、标签和运行环境由所属目录的 `CMakeLists.txt` 管理。

```text
tests/
├─ core/ backend/ renderer/ render_graph/ math/
├─ material/ scene/ lighting/ pipeline/ window/  # 产品组件测试
│  └─ integration/                              # 组件拥有的 GPU 或跨模块回归
├─ asset_tools/                                 # 资产 SDK 与命令行工具
├─ integrations/                                # SDL3、ImGui 等第三方边界
├─ contracts/
│  ├─ headers/                                  # C11/C++20 公共头文件与布局检查
│  └─ abi/                                      # 原生 ABI 快照与导出符号
├─ packaging/                                   # 构建树、安装包和独立 Consumer
├─ smoke/                                       # 最短原生端到端健康检查
├─ web/                                         # Emscripten 与浏览器验证
├─ fixtures/                                    # 输入和已审查的生成资产
├─ support/                                     # 共享辅助代码、参考实现与生成规则
└─ cmake/                                       # 跨目录使用的检查脚本
```

新增测试时遵循以下归属规则：

- 单组件行为放入该组件目录；需要 GPU 或多个产品模块时放入组件的 `integration/`。
- 只有无法归属于单个组件的最短端到端检查进入 `smoke/`。
- 公共头文件、ABI、安装包和第三方库边界分别进入 `contracts/`、`packaging/` 和
  `integrations/`。
- 共享源码和数据不是测试入口，分别放入 `support/` 与 `fixtures/`。

CTest 名称使用 `granit.<组件>.<行为>`。标签只描述运行条件：`unit`、`gpu`、`integration`、
`platform`、`browser`、`package`、`release`、`smoke` 和 `tooling`，不用于表达代码归属。
