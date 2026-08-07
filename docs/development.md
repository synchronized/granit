<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 开发规范

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

## 提交前检查

```sh
cmake --build --preset <preset>
ctest --preset <preset>
```

修改公共 API 时，同时检查 C 和 C++ 独立使用场景，并评估 ABI、所有权、线程安全和句柄失效语义。

测试使用 Catch2 3。C API 需要保留由 C 编译器构建的独立测试，确保 `.h` 公共入口没有意外引入
C++ 语法；行为测试可以使用 Catch2 编写。

新增公共 `.h` 或 `.hpp` 时，应在 `tests/headers/` 增加单独的编译单元，不能只通过总入口头文件
间接验证。
