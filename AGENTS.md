<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# AGENTS.md

## 适用范围

本文件适用于仓库根目录及其所有子目录。所有自动化代理、协作者和代码生成工具在修改本项目时
都应遵守这些约定。子目录如有更具体的 `AGENTS.md`，其规则仅覆盖对应目录，并与本文件共同生效。

## 项目概览

Granit 是一个基于 Vulkan 的 C++20 渲染库，面向游戏引擎、实时应用和图形工具开发。
项目使用稳定的 C ABI 隔离动态库边界，并在其上提供现代 C++20 包装。Vulkan 是内部实现细节，
普通用户不应接触 Vulkan 类型、句柄或生命周期管理。

项目当前处于初始设计阶段，公共 API 和 ABI 尚未稳定。修改时优先保证封装边界、所有权、
错误语义和长期架构清晰，不要为了短期便利泄漏底层实现。

## 通用沟通要求

- 对外说明、进度更新和总结使用中文。
- 代码注释和文档使用中文。
- 回答保持简洁、直接、可执行。
- 不展示内部思考过程；需要解释时提供简要判断依据或排查摘要。
- 不确定的设计问题应先说明假设和影响，不要将猜测写成已经确定的项目事实。

## 核心架构约束

### 公共接口分层

- `.h` 文件提供 C API 和动态库 ABI，必须能由 C11 编译器独立包含。
- `.hpp` 文件提供 C++20 包装，内部调用 C API，面向普通 C++ 用户提供强类型、移动语义和 RAII。
- `include/granit/core` 保存基础句柄、结果码和版本；`include/granit/math` 保存跨 ABI 数学值类型；
  `include/granit/renderer` 保存 GPU 与渲染接口。
- 根级 `include/granit/granit.h` 和 `granit.hpp` 只作为聚合入口，不继续堆放功能头文件。
- C++ 包装层保持轻量，不建立与 C API 平行的第二套运行时状态。
- C API 使用 `granit_` 前缀，宏使用 `GRANIT_` 前缀；C++ API 位于 `granit` 命名空间。
- 普通用户入口不得要求包含或直接调用 Vulkan API。

### Vulkan 封装边界

- `include/` 中不得包含 Vulkan SDK 头文件。
- 公共 API 不得声明、接收或返回 `Vk*` 类型及 Vulkan 枚举、标志或句柄。
- Vulkan 相关声明和实现只能位于 `src/` 内部目录。
- 公共目标不得将 Vulkan include 目录、编译定义或链接依赖传播给使用者。
- 如需原生 Vulkan 互操作，必须作为独立且明确标记的不稳定高级接口设计，不能污染基础 API。

### ABI 与所有权

- C ABI 只使用定宽整数、显式布局的 C 结构体、函数指针和整数句柄。
- ABI 中不得出现 STL 类型、C++ 对象、模板、异常、虚函数或平台宽度不固定的裸 `long`。
- 不跨动态库边界由一侧分配、另一侧直接释放内存；需要跨边界时提供成对函数或显式分配器接口。
- 错误通过结果码返回，不允许异常穿过 C ABI。
- 字符串和数组使用“指针 + 长度”，并在文档中明确所有权和有效期。
- 可扩展描述结构应包含 `struct_size`；字段只在末尾追加，不重排已发布字段。
- 回调必须说明调用线程、可重入性、用户数据所有权和有效期。

### 资源句柄

- renderer、buffer、texture、shader、pipeline、swapchain 和 fence 等资源使用 64 位整数句柄。
- 零值统一表示无效句柄；句柄值不得直接暴露指针、Vulkan 句柄或可持久化身份。
- 句柄表至少校验槽位 generation、资源类型和所属 renderer/device。
- 销毁资源后必须使旧句柄失效，防止槽位复用导致悬空句柄访问新资源。
- 颜色、范围、尺寸、viewport 和创建描述等值数据使用普通结构体，不应无条件转换成句柄。
- 新增资源 API 时必须明确创建、销毁、所有权、线程安全和父资源失效行为。

## 代码规范

- C++ 实现和包装使用 C++20；C 公共接口保持 C11 可表达性。
- 公共 API、文件名、函数和变量使用小写下划线命名。
- 排版遵循 `.clang-format`，每行最多 100 个字符，使用两个空格缩进。
- 仓库文本文件统一使用 LF (`\n`) 换行。
- 优先使用标准库和 RAII，避免不必要的裸 `new` / `delete`。
- 仅在单个 `.cpp` 内使用的自由函数、变量和辅助类型放入匿名 namespace。
- 公共接口应清晰表达所有权、生命周期、错误和线程安全约束。
- 新增复杂逻辑时补充简短中文注释，重点解释约束和原因，不重复代码本身。

## 注释规范

- 源文件和头文件开头保留统一的 SPDX 许可证和版权声明。
- 不在文件头添加个人作者、创建日期或修改记录。
- 公共 API 使用简洁的中文 Doxygen 风格注释。
- 公共注释优先说明用途、参数、返回值、所有权、生命周期、线程安全、失败行为和限制。
- `.cpp` 不要求逐函数注释；跨平台差异、同步、资源状态、内存布局和复杂算法需要说明原因。

## CMake 规范

- 使用现代 target-based CMake，不在全局范围滥用编译选项、宏和包含目录。
- 依赖和属性通过 `target_sources`、`target_include_directories`、`target_link_libraries` 和
  `target_compile_features` 等目标命令声明。
- 公共库目标为 `granit`，使用者通过别名目标 `granit::granit` 链接。
- 默认构建共享库；静态构建通过 `BUILD_SHARED_LIBS=OFF` 或带 `static` 的 preset 选择。
- 公开符号使用 `GRANIT_API`；静态构建必须正确传播 `GRANIT_STATIC_DEFINE`。
- 自有目标使用 `granit_target_compile_warnings`，不得将警告选项传递给第三方库或下游使用者。
- 不随意删除或改名已有 preset；新增平台、编译器或链接模式时同步维护 configure、build 和 test preset。
- 新增公共头文件时同步加入安装 `FILE_SET`，并确认生成头与安装路径正确。
- 测试依赖不得进入 Granit 的安装导出或成为使用者的传递依赖。

## 第三方依赖

- 引入第三方依赖前说明用途、版本、许可证、替代方案和对构建及 ABI 的影响。
- C API 测试使用 Unity，C++ 包装层和内部实现测试使用 Catch2 3；均优先复用父项目目标，
  其次使用 `find_package`，最后回退仓库内置版本。
- 不直接修改内置第三方源码；需要调整警告或编译选项时在 Granit 的 CMake 包装层处理。
- Vulkan SDK 只能是内部实现依赖，不得成为公共头文件依赖。
- Vulkan-Headers 与 Volk 必须锁定匹配版本并成对升级；不得移除编译期版本检查。

## 构建与测试

查看当前平台可用 preset：

```sh
cmake --list-presets
```

Windows Visual Studio 2022 动态库：

```powershell
cmake --preset windows-vs2022-debug
cmake --build --preset windows-vs2022-debug
ctest --preset windows-vs2022-debug
```

Windows Clang + Ninja 动态库：

```powershell
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug
ctest --preset windows-clang-debug
```

Windows 静态库示例：

```powershell
cmake --preset windows-vs2022-static-debug
cmake --build --preset windows-vs2022-static-debug
ctest --preset windows-vs2022-static-debug
```

Linux 示例：

```sh
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug
ctest --preset linux-clang-debug
```

仓库开发 preset 默认将警告视为错误。修改代码后应运行与改动相关的构建和测试；涉及 ABI、
公共头文件、导出宏或 CMake 安装时，应同时验证共享库、静态库、C consumer 和 C++ consumer。
若环境缺失导致无法验证，最终说明必须列出未验证项和原因。

## 测试要求

- 新增 API、修复缺陷或改变行为时，应补充对应测试。
- `.h` 公共入口必须保留由 C 编译器构建的测试，公开 C API 的行为测试使用 Unity。
- C++ 包装的行为测试使用 Catch2，并覆盖成功路径、失败路径和生命周期边界。
- 句柄测试至少覆盖无效值、类型错误、generation 失效、跨设备混用和重复销毁。
- 动态库测试必须保证运行时能够定位 Granit DLL/SO/dylib，不依赖开发机全局路径污染。
- 不通过关闭警告、跳过失败测试或降低检查等级来掩盖问题。

## 修改原则

- 保持改动聚焦，避免无关重构和大范围格式化。
- 不回退用户已有修改，除非用户明确要求。
- 文档必须反映仓库当前实际状态，不把路线图能力描述为已经实现。
- 修改公共 API 时同步更新 C API、C++ 包装、测试和相关文档。
- 优先扩展现有抽象，不创建含义重叠的第二套接口或状态系统。
- 高频渲染操作应考虑批量记录和提交，避免为每个细粒度动作增加一次动态库调用。

## 推荐工作流

1. 修改前阅读相关源码、测试、构建文件和文档，确认边界及验证方式。
2. 检查工作区已有修改，避免覆盖无关内容。
3. 实施最小必要改动，并同步补充测试和文档。
4. 运行格式检查、相关构建和测试；高风险改动扩大验证矩阵。
5. 提交前运行 `git diff --check` 并检查提交范围。
6. 只有用户明确要求时才提交、推送或创建 Pull Request。
7. 最终总结说明变更内容、验证结果、未验证项和剩余风险。
