<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 01：创建第一个 Renderer

本教程从已安装的 Granit C++20 包开始，创建一个不启用窗口呈现的 Renderer，查询设备信息与公开
限制，然后按 RAII 生命周期退出。完成后可以继续学习后续的资源上传教程。

完整可运行程序位于
[`examples/samples/minimal_renderer`](../../examples/samples/minimal_renderer)，对应目标为
`granit_minimal_renderer_example`。本页只解释关键步骤，不复制完整源文件。

## 前置条件

- CMake 3.23 或更高版本；
- 支持 C++20 的编译器；
- 已安装 Granit 及其运行时依赖；
- 可用的 Vulkan 设备和 loader。

安装、共享库路径和静态库配置见[构建与安装](../guides/build.md)。

## 1. 链接 Granit

使用者只需链接公共 C++ 目标，不需要包含 Vulkan 头文件，也不需要链接 Vulkan SDK：

```cmake
find_package(granit CONFIG REQUIRED)

add_executable(minimal_renderer main.cpp)
target_compile_features(minimal_renderer PRIVATE cxx_std_20)
target_link_libraries(minimal_renderer PRIVATE granit::granit)
```

`granit::granit` 是核心 C API 和 C++20 包装的安装目标。窗口、RenderPipeline 和 AssetTools
属于独立 component，后续教程再按需加入。

## 2. 创建 Renderer

最小描述只需要应用名称和呈现模式。这里显式关闭呈现，表示程序不创建 Window、Surface 或
Swapchain：

```cpp
#include <granit/granit.hpp>

granit::renderer renderer;
const granit::renderer_desc description{
    .application_name = "Granit Minimal Renderer",
    .presentation = granit::presentation_mode::disabled,
};

const auto result = renderer.initialize(description);
if (result.failed()) {
  // result.message() 返回适合诊断的稳定错误文本。
  return 1;
}
```

`renderer` 不可复制，只能移动；离开作用域时会自动销毁。`initialize()` 失败时不会抛出异常，
调用方应检查 `granit::result` 并决定是否继续。

## 3. 查询设备信息和限制

Renderer 创建成功后，可以查询实际后端、Adapter 和设备限制：

```cpp
granit::renderer_info info;
if (renderer.get_info(info).failed()) {
  return 1;
}

granit::renderer_limits limits;
if (renderer.get_limits(limits).failed()) {
  return 1;
}

std::cout << info.adapter_name << '\n';
std::cout << limits.max_sampler_anisotropy << '\n';
```

这些值是当前设备的能力快照，不能写成所有设备都相同的常量。需要可选能力时，应通过
`renderer_limits` 的查询函数判断，而不是根据 Vulkan 或 WebGPU 后端名称推断。

更完整的字段、错误和线程约束见[Renderer 参考](../reference/renderer.md)与[线程安全约定](../reference/thread-safety.md)。

## 4. 构建和运行

在源码树中构建最小示例：

```powershell
cmake --preset windows-vs2022-debug
cmake --build --preset windows-vs2022-debug --target granit_minimal_renderer_example
build/windows-vs2022-debug/bin/Debug/granit_minimal_renderer_example.exe
```

成功时会输出 Granit 版本、实际后端、Adapter 名称和最大采样器各向异性。不同机器的 Adapter
和限制值可能不同，不应把它们作为固定快照比较。

也可以执行示例 Smoke：

```powershell
ctest --test-dir build/windows-vs2022-debug -C Debug `
  -R "^granit\.example\.minimal_renderer$" --output-on-failure
```

## 常见问题

- **找不到 DLL**：从构建目录运行时，确保 Granit DLL 与示例位于同一输出目录；CTest 会自动设置
  对应的运行时库搜索路径。
- **创建失败**：检查 Vulkan loader、驱动和设备是否可用；不要把失败转换成成功继续执行。
- **需要窗口**：本教程没有启用呈现。窗口、Surface 和 Swapchain 将在后续的窗口与帧循环教程中介绍。
