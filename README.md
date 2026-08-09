<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# granit

Granit 是一个基于 Vulkan 的 C++20 渲染库，面向游戏引擎、实时应用和图形工具开发。
项目通过稳定的 C ABI 隔离动态库边界，并在其上提供现代 C++20 RAII 包装；Vulkan 始终是
内部实现细节，普通用户无需接触 Vulkan 类型、句柄和生命周期管理。

## 项目定位

Granit 不是 Vulkan API 的逐项重命名或薄包装。项目希望围绕实际渲染任务建立高层抽象，
统一管理设备、资源、同步、命令提交和交换链，同时保留现代图形 API 明确的资源生命周期语义。

核心定位是面向自研引擎和图形工具的中层“Bring Your Own Engine”渲染库：当前只实现 Vulkan，
目标同时覆盖窗口与离屏渲染，但不在核心层接管 Scene、Camera、Light 或完整 PBR 工作流。

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

仓库当前已建立工程、ABI、Vulkan 后端和窗口输出基础：

- `granit` CMake 目标及 `granit::granit` 别名目标。
- `.h` C API 与 `.hpp` C++20 包装的目录约定。
- 动态库导出宏和静态库编译定义。
- 基础版本查询 API。
- 定宽结果码、错误文本和 64 位基础句柄类型。
- 内部非拥有句柄表，支持 generation、资源类型和 domain 校验。
- 内置 Vulkan-Headers 1.4.350 与 Volk 1.4.350，作为不传播给使用者的内部依赖。
- 线程安全的 Vulkan Loader 初始化、Vulkan 1.3 检查和无窗口 instance RAII。
- 确定性的物理设备筛选、逻辑设备、graphics queue 和独立 device 函数表。
- `granit_renderer` C API 与无异常、move-only 的 C++20 RAII 包装。
- Win32 Surface C API 与 RAII 包装，支持按需启用平台 Instance 扩展。
- Swapchain 创建、查询、重建和级联生命周期管理。
- Command Recorder 创建、空命令录制、重置和级联生命周期管理。
- Unity 纯 C API 测试与 Catch2 3 C++ 测试，可复用父项目目标并回退到仓库内置版本。
- CMake 配置、构建、安装和包导出入口。
- 代码格式、静态检查和仓库忽略规则。

GPU 内存分配、Buffer 同步上传、Texture、Texture View、Sampler、统一 Attachment 值类型、
Command Recorder、Buffer Copy/Fill、Dynamic Rendering 和内部帧同步对象已经实现。下一阶段将
增加 Queue 提交与可配置 frames-in-flight；
资源屏障、Swapchain acquire/present 和 Pipeline 尚未实现，具体顺序及验收标准见
[docs/roadmap.md](docs/roadmap.md)。

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

创建 renderer：

```cpp
granit::renderer renderer;
const auto result = renderer.initialize({.application_name = "example"});
if (granit::failed(result)) {
  // 处理错误
}
```

## 文档

- [docs/architecture.md](docs/architecture.md)：分层、ABI、句柄和 Vulkan 封装边界。
- [docs/buffer.md](docs/buffer.md)：Buffer 创建、映射和生命周期。
- [docs/build.md](docs/build.md)：环境要求、构建选项和安装方式。
- [docs/development.md](docs/development.md)：代码风格、命名和目录规范。
- [docs/roadmap.md](docs/roadmap.md)：分阶段路线图。
- [docs/plans/README.md](docs/plans/README.md)：开发计划的状态、命名和维护规则。
- [docs/plans/R-01-memory-allocation.md](docs/plans/R-01-memory-allocation.md)：GPU 内存分配计划。
- [docs/plans/R-02-resource-model.md](docs/plans/R-02-resource-model.md)：第一版资源模型计划。
- [docs/plans/R-03-buffer.md](docs/plans/R-03-buffer.md)：Buffer 生命周期与映射计划。
- [docs/plans/R-04-buffer-upload.md](docs/plans/R-04-buffer-upload.md)：Buffer 同步上传计划。
- [docs/plans/R-05-texture-view.md](docs/plans/R-05-texture-view.md)：Texture 与 View 计划。
- [docs/plans/R-06-sampler.md](docs/plans/R-06-sampler.md)：Sampler 生命周期与能力计划。
- [docs/plans/R-07-swapchain-backbuffer.md](docs/plans/R-07-swapchain-backbuffer.md)：Backbuffer 资源计划。
- [docs/plans/V-01-lifetime-validation.md](docs/plans/V-01-lifetime-validation.md)：生命周期验证计划。
- [docs/plans/R-08-deferred-destruction.md](docs/plans/R-08-deferred-destruction.md)：延迟销毁计划。
- [docs/plans/R-09-render-target-attachment.md](docs/plans/R-09-render-target-attachment.md)：渲染附件计划。
- [docs/plans/F-01-command-recorder.md](docs/plans/F-01-command-recorder.md)：命令录制器计划。
- [docs/plans/F-02-command-recording.md](docs/plans/F-02-command-recording.md)：基础命令录制计划。
- [docs/renderer.md](docs/renderer.md)：公共 renderer C/C++ API 与生命周期。
- [docs/render-target.md](docs/render-target.md)：颜色与深度/模板 Attachment 值类型。
- [docs/command-recorder.md](docs/command-recorder.md)：Command Recorder 状态与线程模型。
- [docs/resource-types.md](docs/resource-types.md)：Buffer、Texture、View 和 Sampler 值类型。
- [docs/sampler.md](docs/sampler.md)：Sampler 状态、能力限制和生命周期。
- [docs/surface.md](docs/surface.md)：窗口 Surface、平台句柄和生命周期。
- [docs/texture.md](docs/texture.md)：Texture、Texture View 和父子生命周期。
- [docs/swapchain.md](docs/swapchain.md)：交换链配置、重建和生命周期。
- [docs/vulkan.md](docs/vulkan.md)：Vulkan loader、instance 和后端边界。
- [3rd/README.md](3rd/README.md)：第三方依赖版本、来源和用途。

## 许可证

Granit 使用 MIT License 发布。详细内容见 [LICENSE](LICENSE)。
