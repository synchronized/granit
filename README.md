<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Granit

Granit 是一个基于 Vulkan 的 C++20 渲染库，面向游戏引擎、实时应用和图形工具开发。项目通过
C ABI 隔离动态库边界，并在其上提供现代 C++20 RAII 包装；普通用户无需接触 Vulkan 类型、
句柄和生命周期管理。

> **开发状态：早期开发。** 当前版本不保证 API 或 ABI 稳定，也不承诺向后兼容。

## 项目定位

Granit 不是 Vulkan API 的逐项重命名，而是围绕实际渲染任务提供分层能力：

```text
Scene / View / Material
          -> Render Pipeline（默认整帧策略）
          -> Render Graph（Pass 与资源依赖）
          -> Renderer / Command Recorder（GPU 执行）
          -> Vulkan（内部后端）
```

使用者可以按需要选择层级：

- 使用完整 Render Pipeline，快速获得默认 Forward PBR 渲染路径。
- 扩展默认 Pipeline 和 Material，插入自定义效果。
- 自行组合 Render Graph 与可选高层模块。
- 直接使用 Renderer，控制资源、命令、同步和提交。

Granit 采用“Bring Your Own Engine”边界，不接管使用者的 ECS、Scene Graph、资产数据库或完整
应用生命周期。

## 核心特点

- **隐藏 Vulkan**：公共头文件不包含 Vulkan，也不暴露 `Vk*` 类型或句柄。
- **C ABI 边界**：`.h` 提供 C11 可表达接口，适合动态库和其他语言绑定。
- **现代 C++20 包装**：`.hpp` 提供强类型、移动语义和 RAII。
- **安全资源句柄**：64 位整数句柄带有类型、所属 Renderer 和 generation 校验。
- **窗口与离屏统一**：Texture View 和 Swapchain Backbuffer 使用同一 Attachment 模型。
- **共享库优先**：默认构建动态库，也支持静态库配置。
- **第三方集成可选**：SDL3 Window 可转换为 Granit Surface，ImGui Draw Data 可转换为 Canvas；
  两者均不成为核心 Renderer 的依赖。
- **可选高层能力**：Material、Scene、PBR、Lighting、Render Graph 和 Render Pipeline 不反向污染
  核心 Renderer。

当前实现能力与后续顺序以[路线图](docs/roadmap.md)为准。

## 快速开始

构建需要 CMake 3.23+、支持 C++20 的 MSVC/Clang/GCC，以及 Ninja 或 Visual Studio 2022。
仓库已内置匹配版本的 Vulkan-Headers 与 Volk；编译不要求完整 Vulkan SDK，运行时仍需可用的
Vulkan loader 和显卡驱动。

查看当前平台可用的 CMake preset：

```sh
cmake --list-presets
```

Windows Visual Studio 2022 动态库构建：

```powershell
cmake --preset windows-vs2022-debug
cmake --build --preset windows-vs2022-debug
ctest --preset windows-vs2022-debug
```

Linux Clang 动态库构建：

```sh
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug
ctest --preset linux-clang-debug
```

构建完成后可按需安装库、公共头文件和 CMake package：

```sh
cmake --install build/<preset-name> --prefix build/install
```

顶层构建默认同时生成示例。Windows Clang 配置完成后可运行最小离屏清屏示例：

```powershell
build/windows-clang-debug/bin/granit_offscreen_clear_example.exe
```

完整的环境要求、构建选项、静态库和安装说明见[构建与安装](docs/guides/build.md)；其他可运行
程序及平台差异见[示例程序](docs/guides/examples.md)。

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

使用可选 Window component：

```cmake
find_package(granit CONFIG REQUIRED COMPONENTS Window)
target_link_libraries(your_target PRIVATE granit::granit granit::window)
```

`granit::window` 是 Granit 自带的可选窗口组件，不是核心 Renderer 的强制依赖。应用也可以自行
接入 SDL3 或 GLFW，具体边界和接入方式见
[SDL3 与 GLFW 窗口接入](docs/guides/window-library-integration.md)。

使用可选 SDL3 和 ImGui Integration：

```cmake
find_package(granit CONFIG REQUIRED COMPONENTS IntegrationSDL3 IntegrationImGui)
target_link_libraries(
  your_target
  PRIVATE
    granit::integration_sdl3
    granit::integration_imgui
)
```

源码树构建时，这两个组件默认关闭。父项目可提供 SDL3 3.2+ 与 ImGui 目标；本仓库开发和验证也可
显式启用锁定依赖：

```sh
cmake -S . -B build/integrations \
  -DGRANIT_BUILD_INTEGRATION_SDL3=ON \
  -DGRANIT_BUILD_INTEGRATION_IMGUI=ON \
  -DGRANIT_FETCH_INTEGRATION_DEPENDENCIES=ON
```

锁定依赖模式仅用于源码树构建与验证，不安装 Integration component。安装这两个 component 时，
应由父项目提供依赖目标，或提供可由 `find_package` 找到的依赖包。

SDL3 Integration 只负责从 `SDL_Window` 创建 Granit Surface；SDL3 继续拥有窗口、事件循环和输入。
ImGui Integration 只负责把 Draw Data 追加到 Canvas，不管理 ImGui Context、字体 Atlas、输入注入或
平台窗口。完整接口与当前限制见
[SDL3 与 ImGui Integration](docs/reference/third-party-integrations.md)。Granit 原生 Input component
尚未实现，当前设计边界见[路线图](docs/roadmap.md)。

C 用户包含 `<granit/granit.h>`，C++20 用户包含 `<granit/granit.hpp>`。

## 文档

使用指南、API 参考、架构说明、路线图和开发计划统一收录在
[Granit 文档中心](docs/README.md)。

项目文档的职责、模板和维护规则见[项目文档规范](DOCUMENTATION_GUIDE.md)。

## 许可证

Granit 使用 MIT License 发布，详见 [LICENSE](LICENSE)。
