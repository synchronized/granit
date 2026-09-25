<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 稳定候选契约证据

本文汇总 Core、RenderPipeline 与 Window 进入稳定版本规划前使用的验证证据。它说明当前门禁覆盖
什么，不构成 0.x ABI 冻结承诺；版本兼容规则仍以[版本与兼容策略](compatibility.md)为准。

## 证据模型

稳定候选需要同时满足四类检查：

| 证据 | 防止的问题 | 当前入口 |
|---|---|---|
| 导出符号快照 | 公共 C 函数遗漏导出、意外删除或私有符号泄漏 | `tests/contracts/abi/snapshots` |
| C 结构布局快照 | 大小、字段偏移、对齐和版本尺寸漂移 | `tests/contracts/abi/*_layout_h.c` |
| 运行时行为契约 | 错误分类、失败输出、句柄归属和生命周期变化 | `granit.contracts.stable_candidate` 及各 component 测试 |
| 安装 SDK Consumer | 构建树依赖泄漏、CMake 目标错误和 C/C++ 包装失配 | `tests/packaging/consumer` |

符号快照记录完整公共函数集合；跨版本检查要求稳定候选 component 的当前集合包含历史基线。结构
布局快照绑定 Granit 版本、平台、架构和编译器范围，当前覆盖 Windows/Linux x86_64 上的 MSVC、
Clang 和 GCC。公共头编译测试继续覆盖所有版本尺寸宏，0.35.0 的 component 快照额外固定关键拥有
型、借用型和逐帧结构的数值布局。

行为契约验证输入描述先于后端执行得到检查、创建失败清空输出句柄，以及合法描述配合无效所有者
返回 `GRANIT_ERROR_INVALID_HANDLE`。更完整的类型、generation、跨 Renderer、重复销毁和父对象
级联行为由 Core、Renderer、RenderPipeline 与 Window 的专项测试覆盖。

## Component 结论

| Component | 当前结论 | 主要依据 | 进入稳定规划前仍需确认 |
|---|---|---|---|
| Core | 已具备稳定范围评审条件 | 0.29.0 跨版本符号基线、0.35.0 布局快照、资源与句柄行为、C11/C++20 Consumer | 明确首个稳定版本包含的功能集合和弃用策略 |
| Window | 已具备稳定范围评审条件 | 0.25.0 跨版本符号基线、Window/Input 布局、线程与输出容量测试、C11/C++20 Consumer | 明确各平台 Backend 的支持等级和宿主生命周期限制 |
| RenderPipeline | 保持稳定候选 | 0.29.0 跨版本符号基线、0.35.0 布局、资产/场景/渲染行为与 Consumer | 继续观察真实上游对 Material、Scene 与同步 render 门面的演进需求 |
| AssetTools | 实验性 | 精确符号快照、显式破坏清单和安装 Consumer | 离线工具链仍允许有记录的 0.x 破坏性调整 |

“具备稳定范围评审条件”表示自动化证据足以开始确定冻结范围，仍不表示接口已经冻结。正式稳定承诺
必须在发布说明中逐 component 宣布，并从该版本开始把兼容基线切换到首个稳定版本。

## 修改规则

- 为稳定候选 component 增加 C 函数时，更新当前精确符号快照；不得删除历史基线中的函数。
- 修改公开结构时，保留旧 `*_VERSION_N_SIZE`，只在尾部追加字段，并同步布局与头文件编译测试。
- 改变所有权、线程、错误或输出语义时，同一变更必须更新对应 Reference 和行为测试。
- C++ 包装只建立源码兼容证据；它必须继续通过公开 C API，不保存平行的运行时状态。
- 新平台或架构不能直接复用现有数字布局，必须先建立独立快照身份和 CI 验证。
