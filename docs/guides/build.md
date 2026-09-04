<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 构建与安装

## 环境要求

- CMake 3.23 或更高版本。
- 支持 C++20 的编译器：MSVC、Clang 或 GCC。
- Ninja 或 Visual Studio 2022。

- 仓库内置 Vulkan-Headers 1.4.350 和 Volk 1.4.350，编译 Granit 不要求安装完整 Vulkan SDK。
- 运行 Vulkan 后端仍需要操作系统中可用的 Vulkan loader 和显卡驱动。

## CMake 选项

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `BUILD_SHARED_LIBS` | `ON` | 构建共享库；设为 `OFF` 时构建静态库 |
| `GRANIT_BUILD_TESTING` | `ON` | 构建测试 |
| `GRANIT_BUILD_EXAMPLES` | 顶层项目为 `ON` | 构建示例 |
| `GRANIT_BUILD_BENCHMARKS` | `OFF` | 构建独立性能基准程序 |
| `GRANIT_BUILD_TOOLS` | `OFF` | 单独构建离线工具；示例或 benchmark 会自动构建所需工具 |
| `GRANIT_BUILD_SHADER_TOOLS` | `OFF` | 构建并安装可供编辑器链接的 ShaderTools SDK |
| `GRANIT_ENABLE_XCB` | Linux 上 `ON` | 找到 XCB 开发头时启用私有 XCB Surface 后端 |
| `GRANIT_ENABLE_WAYLAND` | Linux 上 `ON` | 找到 Wayland 协议工具时启用 Wayland 后端 |
| `GRANIT_ENABLE_WARNINGS` | `ON` | 为 Granit 自有目标启用编译警告 |
| `GRANIT_ENABLE_PEDANTIC_WARNINGS` | `OFF` | 启用 `-Wpedantic` 等严格标准扩展警告 |
| `GRANIT_WARNINGS_AS_ERRORS` | `OFF` | 将 Granit 自有源码警告视为错误 |

仓库提供的开发 presets 会将 `GRANIT_WARNINGS_AS_ERRORS` 设为 `ON`，以便尽早发现问题。
作为子项目手动引入时默认保持 `OFF`，并且所有警告选项均为目标私有属性，不会传递给使用者。

Wayland Window 需要 `wayland-client`、`wayland-scanner` 和 `wayland-protocols`；Wayland Input
额外查找 `libxkbcommon`。缺少 `libxkbcommon` 时只禁用 Wayland Input，XCB Input 和 Wayland
Window 仍可构建。上述库均不进入 Granit 公共头文件；静态链接 Input 时最终应用仍需链接系统
`libxkbcommon`。

桌面平台只构建 Vulkan 后端，不需要 Dawn SDK。浏览器 WebGPU 由 Emscripten 构建静态接入，
其环境和运行方法见[浏览器 WebGPU 示例](webgpu-browser-example.md)。

## 测试依赖

公开 C API 测试使用 Unity 2.6，C++20 包装层和内部实现测试使用 Catch2 3。配置时优先复用
父项目已有目标，其次通过 `find_package` 查找安装包，最后回退到仓库内置的 Unity 2.6.1 和
Catch2 3.15.0。两套框架只在同时启用 `GRANIT_BUILD_TESTING` 和 CMake `BUILD_TESTING`
时引入，不会成为 Granit 安装包或使用者的传递依赖。

Vulkan-Headers 与 Volk 始终作为 Granit 内部构建依赖。Volk 对象直接并入库目标，不出现在
安装后的 CMake package 中；Vulkan include 目录、`VK_NO_PROTOTYPES` 和 `VOLK_NAMESPACE`
也不会传播给使用者。

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

使用可选参考渲染管线：

```cmake
find_package(granit CONFIG REQUIRED COMPONENTS RenderPipeline)
target_link_libraries(your_target PRIVATE granit::render_pipeline)
```

`RenderPipeline` component 同时安装 C API 与 C++20 包装，支持共享库和静态库。静态内部依赖由
CMake 导出自动闭包；使用者不应直接依赖 `granit::detail_*` 目标。

外部配置时将安装前缀加入 `CMAKE_PREFIX_PATH`：

```sh
cmake -S consumer -B consumer/build -DCMAKE_PREFIX_PATH=/path/to/granit/install
```

仓库提供独立的 C11/C++20 Consumer 验证工程，可用于检查安装包而不访问源码目标：

```sh
cmake -S tests/consumer -B build/consumer -DCMAKE_PREFIX_PATH=/path/to/granit/install
cmake --build build/consumer
ctest --test-dir build/consumer --output-on-failure
```

Consumer CTest 会运行 Core C/C++、RenderPipeline C/C++、Window C 和 Input C/C++ 七条路径，
并从安装目标自动补充共享库搜索路径；不需要把 DLL 或 SO 复制进 Consumer 构建目录。

维护安装规则时还可运行包选择测试，确认兼容与精确版本能够找到包，而错误主版本和未知必需
component 会被拒绝：

```sh
cmake -DGRANIT_SOURCE_DIR=/path/to/granit \
  -DGRANIT_INSTALL_PREFIX=/path/to/granit/install \
  -DGRANIT_TEST_BINARY_DIR=/path/to/granit/build/package-check \
  -P /path/to/granit/cmake/check_installed_package.cmake
```

安装导出审计用于检查必要文件，并防止源码路径、构建路径、测试库和 Vulkan 私有依赖泄漏：

```sh
cmake -DGRANIT_SOURCE_DIR=/path/to/granit \
  -DGRANIT_BUILD_DIR=/path/to/granit/build \
  -DGRANIT_INSTALL_PREFIX=/path/to/granit/install \
  -P /path/to/granit/cmake/check_install_exports.cmake
```

共享库使用者还需要按照目标平台的部署规则，让运行进程能够找到 DLL、SO 或 dylib。
仓库 CI 当前覆盖 Linux Clang/GCC × 共享/静态，以及 Windows MSVC × 共享/静态安装 Consumer。
Windows 共享验证不会把 DLL 复制到 Consumer 目录，而是从安装前缀的 `bin` 目录加载。

## 安装支持与验证矩阵

| 平台 | 编译器 | 链接模式 | 安装 Consumer | 当前验证状态 |
| --- | --- | --- | --- | --- |
| Windows x64 | MSVC | 共享、静态 | C11、C++20、RenderPipeline、Window、Input | Release CI 已通过 |
| Linux x64 | Clang | 共享、静态 | C11、C++20、RenderPipeline、Window、Input | Release CI 已通过 |
| Linux x64 | GCC | 共享、静态 | C11、C++20、RenderPipeline、Window、Input | Release CI 已通过 |

该矩阵描述当前持续验证范围，不等同于 API 或 ABI 稳定承诺。Windows Clang/clang-cl preset 可用于
开发，但尚未进入安装 Consumer 的必过矩阵。2026-08-18 的跨平台验收结果见
[CI 验证记录](../records/2026-08-18-cross-platform-ci-validation.md)。

## 安装故障排查

- `find_package` 找不到 Granit：将具体安装前缀加入 `CMAKE_PREFIX_PATH`，不要指向源码或构建目录。
- 找不到 `RenderPipeline`：确认执行了完整安装，并使用大小写准确的 `COMPONENTS RenderPipeline`。
- Windows 报 DLL 缺失：将安装前缀的 `bin` 加入进程 `PATH`，或把所需 DLL 随应用一起部署。
- 静态链接出现未解析符号：确认 Consumer 没有混用共享与静态安装前缀；静态宏由导入目标传播，
  不应由使用者手工猜测定义。
- 包版本或运行库版本不一致：删除 Consumer 的 CMake 缓存，重新安装并配置，避免命中旧前缀。
- 多配置生成器安装了错误配置：使用 `cmake --install <build> --config Debug|Release` 明确选择。
- Renderer 返回后端不可用：检查 Vulkan loader 和驱动；这与 CMake 包能否被找到是两个独立问题。

## 运行示例

示例的用途、稳定层级和运行命令见[示例程序](examples.md)。

## 构建产物目录

Granit 只为自己的目标设置输出目录，不修改父项目的全局 CMake 输出变量：

- 单配置生成器：可执行文件和 Windows DLL 位于 `bin`，静态库、导入库及其他库位于 `lib`。
- Visual Studio 等多配置生成器：使用 `bin/<配置>` 和 `lib/<配置>`。

因此 Windows 共享库构建中的程序与 `granit.dll` 位于同一目录，无需修改 `PATH` 即可直接运行。
例如可执行窗口清屏 Smoke：

```powershell
build/windows-clang-debug/bin/granit_window_clear_smoke.exe
build/windows-vs2022-debug/bin/Debug/granit_window_clear_smoke.exe
```
