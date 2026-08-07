<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# granit

Granit 是一个基于 Vulkan 的 C++20 渲染库，面向游戏引擎、实时应用和图形工具开发。
项目通过稳定的 C ABI 隔离动态库边界，并在其上提供现代 C++20 RAII 包装；Vulkan 始终是
内部实现细节，普通用户无需接触 Vulkan 类型、句柄和生命周期管理。

## 项目定位

Granit 不是 Vulkan API 的逐项重命名或薄包装。项目希望围绕实际渲染任务建立高层抽象，
统一管理设备、资源、同步、命令提交和交换链，同时保留现代图形 API 明确的资源生命周期语义。

当前项目处于初始设计阶段，暂不承诺公共 API 或 ABI 稳定性。进入稳定版本前允许根据实现验证
调整接口，以优先保证封装边界、所有权和长期架构清晰。

## 设计原则

- **隐藏 Vulkan**：公共头文件不包含 Vulkan 头文件，也不暴露任何 `Vk*` 类型或句柄。
- **稳定的动态库边界**：`.h` 提供 C ABI，不跨边界传递 STL 类型、异常或 C++ 对象。
- **现代 C++ 接口**：`.hpp` 在 C ABI 之上提供强类型、移动语义和 RAII 包装。
- **安全资源句柄**：有身份和生命周期的资源使用 64 位整数句柄，内部验证类型、归属和 generation。
- **共享库优先**：默认构建动态库；高级用户可通过 CMake 配置构建静态库。
- **渐进式高级能力**：原生 Vulkan 互操作将在基础 API 稳定后作为独立高级接口设计。

架构和 ABI 约束详见 [docs/architecture.md](docs/architecture.md)。

## 当前状态

仓库当前提供项目骨架：

- `granit` CMake 目标及 `granit::granit` 别名目标。
- `.h` C API 与 `.hpp` C++20 包装的目录约定。
- 动态库导出宏和静态库编译定义。
- 基础版本查询 API。
- 定宽结果码、错误文本和 64 位基础句柄类型。
- 内部非拥有句柄表，支持 generation、资源类型和 domain 校验。
- Catch2 3 测试骨架，可复用父项目目标并回退到仓库内置版本。
- CMake 配置、构建、安装和包导出入口。
- 代码格式、静态检查和仓库忽略规则。

renderer 公共 API 和 Vulkan 后端尚未实现。

## 快速开始

查看可用 preset：

```sh
cmake --list-presets
```

Windows 使用 Visual Studio 2022：

```powershell
cmake --preset windows-vs2022-debug
cmake --build --preset windows-vs2022-debug
ctest --preset windows-vs2022-debug
```

Linux 使用 Ninja 与 Clang：

```sh
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug
ctest --preset linux-clang-debug
```

默认构建共享库。静态构建可在配置时覆盖：

```sh
cmake --preset linux-clang-debug -DBUILD_SHARED_LIBS=OFF
```

完整说明见 [docs/build.md](docs/build.md)。

## CMake 集成

安装 Granit 后，外部工程可以通过 Config 模式查找并链接：

```cmake
find_package(granit CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE granit::granit)
```

C 用户包含：

```c
#include <granit/granit.h>
```

C++20 用户包含：

```cpp
#include <granit/granit.hpp>
```

## 文档

- [docs/architecture.md](docs/architecture.md)：分层、ABI、句柄和 Vulkan 封装边界。
- [docs/build.md](docs/build.md)：环境要求、构建选项和安装方式。
- [docs/development.md](docs/development.md)：代码风格、命名和目录规范。
- [docs/roadmap.md](docs/roadmap.md)：分阶段路线图。

## 许可证

Granit 使用 MIT License 发布。详细内容见 [LICENSE](LICENSE)。
