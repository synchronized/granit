<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 构建与安装

## 环境要求

- CMake 3.23 或更高版本。
- 支持 C++20 的编译器：MSVC、Clang 或 GCC。
- Ninja 或 Visual Studio 2022。

Vulkan SDK 将在 Vulkan 后端接入后成为开发依赖，但不会成为公共头文件依赖。

## CMake 选项

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `BUILD_SHARED_LIBS` | `ON` | 构建共享库；设为 `OFF` 时构建静态库 |
| `GRANIT_BUILD_TESTING` | `ON` | 构建测试 |
| `GRANIT_BUILD_EXAMPLES` | 顶层项目为 `ON` | 构建示例 |
| `GRANIT_ENABLE_WARNINGS` | `ON` | 为 Granit 自有目标启用编译警告 |
| `GRANIT_ENABLE_PEDANTIC_WARNINGS` | `OFF` | 启用 `-Wpedantic` 等严格标准扩展警告 |
| `GRANIT_WARNINGS_AS_ERRORS` | `OFF` | 将 Granit 自有源码警告视为错误 |

仓库提供的开发 presets 会将 `GRANIT_WARNINGS_AS_ERRORS` 设为 `ON`，以便尽早发现问题。
作为子项目手动引入时默认保持 `OFF`，并且所有警告选项均为目标私有属性，不会传递给使用者。

## 测试依赖

测试使用 Catch2 3。配置时依次复用父项目已有的 `Catch2::Catch2WithMain`、通过
`find_package(Catch2 3)` 找到的安装包，最后回退到仓库内置的 Catch2 3.15.0。
Catch2 只在同时启用 `GRANIT_BUILD_TESTING` 和 CMake `BUILD_TESTING` 时引入，
不会成为 Granit 安装包或使用者的传递依赖。

## 手动配置

```sh
cmake -S . -B build -DBUILD_SHARED_LIBS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## 安装与使用

```sh
cmake --install build --prefix build/install
```

安装内容包括动态库或静态库、公共头文件、许可证及 CMake package 文件。外部工程使用：

```cmake
find_package(granit CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE granit::granit)
```

外部配置时将安装前缀加入 `CMAKE_PREFIX_PATH`：

```sh
cmake -S consumer -B consumer/build -DCMAKE_PREFIX_PATH=/path/to/granit/install
```

共享库使用者还需要按照目标平台的部署规则，让运行进程能够找到 DLL、SO 或 dylib。
