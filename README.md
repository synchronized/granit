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
- **可选高层能力**：Material、Scene、PBR、Lighting、Render Graph 和 Render Pipeline 不反向污染
  核心 Renderer。

当前实现能力与后续顺序以[路线图](docs/roadmap.md)为准。

## 快速开始

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

完整的环境要求、构建选项、静态库和安装说明见[构建与安装](docs/build.md)。

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

C 用户包含 `<granit/granit.h>`，C++20 用户包含 `<granit/granit.hpp>`。

## 文档

使用指南、API 参考、架构说明、路线图和开发计划统一收录在
[Granit 文档中心](docs/README.md)。

项目文档的职责、模板和维护规则见[项目文档规范](DOCUMENTATION_GUIDE.md)。

## 许可证

Granit 使用 MIT License 发布，详见 [LICENSE](LICENSE)。
