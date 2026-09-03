<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Granit

[![Windows](https://github.com/synchronized/granit/actions/workflows/windows.yml/badge.svg)](https://github.com/synchronized/granit/actions/workflows/windows.yml) [![Linux](https://github.com/synchronized/granit/actions/workflows/linux.yml/badge.svg)](https://github.com/synchronized/granit/actions/workflows/linux.yml) [![Release](https://img.shields.io/github/v/release/synchronized/granit)](https://github.com/synchronized/granit/releases/latest) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)<br>
Granit 是一个支持 Vulkan 与 WebGPU 的 C++20 渲染库，面向游戏引擎、实时应用和图形工具开发。
项目通过 C ABI 隔离动态库边界，并在其上提供现代 C++20 RAII 包装；普通用户无需接触后端原生
类型、句柄和生命周期管理。

> **最新发布版本：0.4.0。** 0.x 不保证 API/ABI 稳定；升级前请阅读
> [变更记录](CHANGELOG.md)和[兼容策略](docs/reference/compatibility.md)。

## 项目定位

Granit 不是 Vulkan API 的逐项重命名，而是围绕实际渲染任务提供分层能力：

```text
Scene / View / Material
          -> Render Pipeline（默认整帧策略）
          -> 内部 Render Graph（Pass 与资源依赖）
          -> Renderer / Command Recorder（GPU 执行）
          -> 私有 HAL -> Vulkan / WebGPU（内部后端）
```

使用者可以按需要选择层级：

- 使用完整 Render Pipeline，快速获得默认 Forward PBR 渲染路径。
- 扩展默认 Pipeline 和 Material，插入自定义效果。
- 直接使用核心 Renderer 自行组织渲染流程；Render Graph 当前仅供参考管线内部使用。

Granit 采用“Bring Your Own Engine”边界，不接管使用者的 ECS、Scene Graph、资产数据库或完整
应用生命周期。

## 核心特点

- **统一多后端接口**：同一套公共 API 驱动 Vulkan 与 WebGPU，且不暴露后端原生类型或句柄。
- **C ABI 边界**：`.h` 提供 C11 可表达接口，适合动态库和其他语言绑定。
- **现代 C++20 包装**：`.hpp` 提供强类型、移动语义和 RAII。
- **安全资源句柄**：64 位整数句柄带有类型、所属 Renderer 和 generation 校验。

当前实现状态与后续顺序见[路线图](docs/roadmap.md)。

## 当前可用模块

| 模块 | CMake 目标 | 稳定性 | 当前范围 |
|---|---|---|---|
| [核心 Renderer](docs/reference/renderer.md) | `granit::granit` | 0.x，未冻结 | GPU 资源、命令、Pipeline、同步与提交 |
| [参考渲染管线](docs/reference/render-pipeline.md) | `granit::render_pipeline` | 0.x，未冻结 | Forward PBR、Lighting、Canvas、Debug Draw 与 Text |
| [Window](docs/reference/window.md) | `granit::window` | 0.x，未冻结 | Win32、XCB、Wayland 窗口、事件及 Surface 接入 |
| [Input](docs/reference/input.md) | `granit::input` | 0.x，未冻结 | Win32、XCB 和 Wayland 输入状态与事件 |
| [第三方集成](docs/reference/third-party-integrations.md) | `granit::integration_sdl3`、`granit::integration_imgui` | 实验性 | SDL3 Surface 与 ImGui Draw Data 转换 |

## 使用发布包

[Granit 0.4.0 Release](https://github.com/synchronized/granit/releases/tag/v0.4.0) 提供 Windows 与
Linux x64 的共享库、静态库安装包及 `SHA256SUMS`。下载后先验证校验和，再解压到固定目录；压缩包
内的顶层目录就是 CMake package 前缀：

```sh
cmake -S <your-source> -B <your-build> -DCMAKE_PREFIX_PATH=<granit-package>
cmake --build <your-build>
```

发布包只包含可分发 SDK；仓库示例及其资源需要从源码构建。
共享库还需位于运行时搜索路径：Windows 将包内 `bin` 加入 `PATH`，Linux 将包内 `lib` 加入
`LD_LIBRARY_PATH` 或按应用部署规则安装。源码构建、依赖要求和静态链接说明见
[构建与安装](docs/guides/build.md)。

## 快速开始

构建需要 CMake 3.23+、支持 C++20 的 MSVC/Clang/GCC，以及 Ninja 或 Visual Studio 2022。
仓库已内置匹配版本的 Vulkan-Headers 与 Volk；默认 Vulkan 构建不要求完整 Vulkan SDK，运行时
仍需可用的 Vulkan loader 和显卡驱动。桌面 Dawn 与 Emscripten WebGPU 的依赖和构建方式见
[构建与安装](docs/guides/build.md)。

查看当前平台可用的 CMake preset：

```sh
cmake --list-presets
```

选择与平台匹配的 preset 进行配置、构建和测试，例如：

```sh
cmake --preset windows-vs2022-debug
cmake --build --preset windows-vs2022-debug
ctest --preset windows-vs2022-debug
```

构建完成后可按需安装库、公共头文件和 CMake package：

```sh
cmake --install build/<preset-name> --prefix build/install
```

顶层构建默认同时生成示例。Windows Clang 配置完成后可运行最小离屏清屏示例：

```powershell
build/windows-clang-debug/bin/granit_offscreen_clear_example.exe
```

完整构建说明见[构建与安装](docs/guides/build.md)，其他程序见[示例程序](docs/guides/examples.md)，
跨后端模型查看器见[Model Viewer 指南](docs/guides/model-viewer.md)，浏览器 WebGPU 基础示例见
[WebGPU 浏览器指南](docs/guides/webgpu-browser-example.md)。

最小 C++20 程序只需包含聚合头并初始化 Renderer：

```cpp
#include <granit/granit.hpp>

#include <iostream>

int main() {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "My Granit App"});
  if (!result) {
    std::cerr << "创建 Renderer 失败: " << result.message() << '\n';
    return 1;
  }

  // renderer 离开作用域时自动释放。
  return 0;
}
```

## CMake 集成

安装后通过 Config 模式链接核心 Renderer：

```cmake
find_package(granit CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE granit::granit)
```

使用可选参考渲染管线：

```cmake
find_package(granit CONFIG REQUIRED COMPONENTS RenderPipeline)
target_link_libraries(your_target PRIVATE granit::render_pipeline)
```

使用可选 Window 和 Input component：

```cmake
find_package(granit CONFIG REQUIRED COMPONENTS Window Input)
target_link_libraries(your_target PRIVATE granit::granit granit::window granit::input)
```

Window 和 Input 均不是核心 Renderer 的强制依赖。应用也可以自行接入 SDL3 或 GLFW，具体边界见
[窗口库接入](docs/guides/window-library-integration.md)；Input 行为见
[Input component](docs/reference/input.md)。

使用可选 SDL3 和 ImGui Integration：

```cmake
find_package(granit CONFIG REQUIRED COMPONENTS IntegrationSDL3 IntegrationImGui)
target_link_libraries(
  your_target PRIVATE granit::integration_sdl3 granit::integration_imgui
)
```

SDL3 Integration 只负责创建 Granit Surface；ImGui Integration 只负责把 Draw Data 追加到 Canvas。
源码树构建时两者默认关闭，安装使用时由父项目或 `find_package` 提供 SDL3 3.2+ 与 ImGui；完整
启用方式、依赖和所有权边界见
[SDL3 与 ImGui Integration](docs/reference/third-party-integrations.md)。
组合示例同时覆盖字体 Atlas 与自定义 Texture ID，并通过 Canvas 的逐帧公共绑定及有界纹理绑定
缓存录制；Canvas 的当前录制语义见 [Canvas Draw List](docs/reference/canvas-draw-list.md)。

## 文档

使用指南、API 参考、架构说明、路线图和开发计划统一收录在
[Granit 文档中心](docs/README.md)。

项目文档的职责、模板和维护规则见[项目文档规范](DOCUMENTATION_GUIDE.md)。

## 许可证

Granit 使用 MIT License 发布，详见 [LICENSE](LICENSE)。
