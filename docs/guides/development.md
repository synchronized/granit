<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 开发规范

项目文档的职责、分类、模板和维护流程统一遵循
[项目文档规范](../../DOCUMENTATION_GUIDE.md)。完整文档入口见[文档中心](../README.md)。

## 公共头文件布局

- `include/granit/core`：导出宏、结果码、基础类型和版本信息，不依赖 Renderer。
- `include/granit/renderer`：Renderer、资源、Pipeline、命令录制、Surface 和 Swapchain。
- `include/granit/granit.h` 与 `granit.hpp`：面向普通用户的聚合入口。
- Window、Asset、Scene 等高层模块只在实际实现时增加目录，底层模块不得反向依赖高层。

当前仍构建单一 `granit::granit` 目标；源码目录分层不等同于拆分动态库。

## 开发阶段兼容策略

当前处于早期开发阶段，兼容范围以[版本与兼容策略](../reference/compatibility.md)为准。
为了修正抽象、所有权和生命周期设计，
0.x 可以进行有记录的破坏性修改，不需要增加旧接口转发、兼容宏或废弃过渡期，但必须同步说明
影响范围、迁移方式和版本变化。

修改后仍应同步更新 C API、C++ 包装、测试、示例和文档，避免仓库内部同时保留多套含义重叠的
接口。该策略只免除对旧版本的兼容义务，不降低定宽 ABI、Vulkan 隔离和接口质量要求。

## 语言与格式

- 公共 C API 使用 C11 可表达的类型；实现和 C++ 包装使用 C++20。
- 使用仓库根目录 `.clang-format`，每行最多 100 个字符，缩进为两个空格。
- 文件名、函数、变量和命名空间使用小写下划线命名。
- C API 使用 `granit_` 前缀；宏使用 `GRANIT_` 前缀。
- C++ API 位于 `granit` 命名空间。
- 代码注释和项目文档使用中文。
- 每个源文件保留 SPDX 许可证头。
- Granit 自有目标统一通过 `granit_target_compile_warnings` 配置警告，第三方目标不应用该函数。

## 公共头文件

- `.h` 文件是 C API，不得依赖 C++ 标准库或 Vulkan。
- `.hpp` 文件是 C++20 包装，可包含对应 `.h` 文件。
- 每个公共头文件必须能够独立包含。
- 公共符号使用 `GRANIT_API`，内部符号不导出。
- C ABI 中禁止使用 `bool`、裸 `long` 和大小随平台变化的类型，优先使用定宽整数。

## 目录约定

```text
cmake/       CMake package 和辅助模块
docs/        设计及使用文档
examples/    可独立运行的最小示例
include/     C 与 C++ 公共头文件
src/         内部实现，包括未来的 Vulkan 后端
tests/       自动化测试
```

Vulkan 相关声明只能位于 `src/` 内部目录，不能通过公共头文件间接泄漏。

Vulkan-Headers 与 Volk 必须锁定同一 registry 版本并成对升级，升级时同步更新
`3rd/README.md` 和编译期版本检查。

## 提交前检查

```sh
cmake --build --preset <preset>
ctest --preset <preset>
```

修改公共 API 时，同时检查 C 和 C++ 独立使用场景，并评估 ABI、所有权、线程安全和句柄失效语义。

公开 C API 的行为测试使用 Unity，并由 C 编译器构建，确保 `.h` 公共入口没有意外引入
C++ 语法。C++20 包装层和内部实现测试使用 Catch2 3。两套测试统一由 CTest 执行。

新增公共 `.h` 或 `.hpp` 时，应在 `tests/headers/` 增加单独的编译单元，不能只通过总入口头文件
间接验证。

## 分支、提交与 Actions

一个完整路线图任务或可独立评审的功能使用一个特性分支，例如
`feat/f10-frame-context`。设计、C ABI、C++ 包装、示例迁移和文档等子阶段应在同一分支连续完成，
不为每个子步骤创建单独 Pull Request。

子阶段完成后可以创建聚焦的本地提交，并运行与改动相关的本地构建和测试。默认等完整任务达到
可评审状态后再统一推送、创建一个 Pull Request 并触发 Actions。这样既保留清晰提交历史，也避免
每个子阶段分别运行 PR 与 `main` 两套重复矩阵。

以下情况允许提前推送到同一特性分支进行阶段验证：

- 本地无法覆盖 Linux、Windows 或特定编译器差异；
- 需要容器、安装 Consumer、图形会话或其他远端集成环境；
- 正在修改 Actions、发布、缓存或镜像配置；
- 用户明确要求立即推送或检查远端结果。

CI 修复继续复用同一 Pull Request，不创建重复 PR。Pull Request Actions 全部通过后再合并，合并后
以 `main` Actions 作为最终确认。推送前应整理提交范围并运行 `git diff --check`；不要通过空提交或
无实质变化的反复推送观察 CI。

纯 `docs/**`、README、CHANGELOG 或文档规范修改只运行轻量 Documentation 工作流；涉及源码、
CMake、测试或工作流配置时仍运行完整 Windows/Linux 矩阵。文档检查脚本本身的修改也会触发
Documentation，避免为了更新说明重复执行编译与安装 Consumer。
