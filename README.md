<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# granit

Granit 是一个基于 Vulkan 的 C++20 渲染库，面向游戏引擎、实时应用和图形工具开发。
项目通过 C ABI 隔离动态库边界，并在其上提供现代 C++20 RAII 包装；Vulkan 始终是
内部实现细节，普通用户无需接触 Vulkan 类型、句柄和生命周期管理。

> **开发状态：早期开发。** 当前版本不保证 API 或 ABI 稳定，也不承诺向后兼容。现阶段可以
> 根据实现验证直接修改、替换或删除公共接口及二进制布局，无需为旧版本保留兼容层；发布稳定
> 版本前将重新定义并执行正式的 API/ABI 兼容策略。

## 项目定位

Granit 不是 Vulkan API 的逐项重命名或薄包装。项目希望围绕实际渲染任务建立高层抽象，
统一管理设备、资源、同步、命令提交和交换链，同时保留现代图形 API 明确的资源生命周期语义。

核心定位是面向自研引擎和图形工具的中层“Bring Your Own Engine”渲染库：当前只实现 Vulkan，
目标同时覆盖窗口与离屏渲染，但不在核心层接管 Scene、Camera、Light 或完整 PBR 工作流。

当前项目处于初始设计阶段。开发决策优先保证封装边界、所有权和长期架构清晰，不因维护尚未
发布的旧接口而阻碍必要调整。

## 设计原则

- **隐藏 Vulkan**：公共头文件不包含 Vulkan 头文件，也不暴露任何 `Vk*` 类型或句柄。
- **清晰的动态库边界**：`.h` 提供 C ABI，不跨边界传递 STL 类型、异常或 C++ 对象；当前布局
  仍可随开发需要改变。
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
Command Recorder、Buffer Copy/Fill、自动资源屏障、Dynamic Rendering、Queue 提交与可配置
frames-in-flight、Frame 令牌以及 Swapchain acquire/submit/present 窗口帧循环已经实现。项目
已支持未提交 Frame 回收、窗口零尺寸暂停语义，以及 Surface/Device Lost 的窗口帧终止状态。
Renderer 全局 Device Lost 门禁、普通 GPU 资源真实提交完成点和 Swapchain presentation 安全
退役均已完成。Shader Module、Graphics Pipeline、Bind Group Layout、Pipeline Layout、不可变
Bind Group、Command Recorder 资源绑定、Viewport、Scissor、Vertex/Index Buffer、Draw、
Compute Pipeline 和 Dispatch 已经实现。仓库已提供离屏清屏、窗口清屏、真实 Vertex Buffer
窗口三角形和 Compute Storage Buffer 示例；Pipeline Cache、并发创建和 Shader 热替换边界已经
完成。阶段六已完成公开对象线程安全矩阵、CPU/Queue/上传性能基线、批量 Recorder 提交和
Buffer/Texture Upload Batch。P-05 已确认当前不引入内部线程池或公开执行器 API，并为未来
Render Graph 记录了外部执行器扩展点和量化重评条件。P-06 已确认 Render Graph 是可选高层
模块，首版采用串行、单队列执行且不做瞬态内存别名。H-01A～H-01D 已完成纯 CPU 图编译、
Buffer/Texture View 导入、单 Recorder 串行执行，以及瞬态资源按首末使用点创建回收；这些原型
当前不进入核心动态库。窗口输出、诊断和性能复核已经完成；现阶段不缓存或并行化，瞬态资源池
留待真实重复帧验证。H-02A～H-02D 已完成材质系统边界、CPU shadow buffer、dirty 区间上传、
Texture/Sampler Bind Group 事务式替换，以及 DXC/SPIR-V 反射工具原型。H-02E1 已完成内存版本化
材质包、稳定变体查找与 Shader/Pipeline 缓存。H-02E3A 已完成持久化包文件头和区段目录解析；
下一步实现编码、SHA-256 与完整结构校验。
具体顺序见 [docs/roadmap.md](docs/roadmap.md)。

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

## 示例

启用 `GRANIT_BUILD_EXAMPLES` 后会构建以下程序：

- `granit_offscreen_clear_example`：创建离屏颜色附件并清屏。
- `granit_offscreen_triangle_example`：使用预编译 SPIR-V 绘制最小三角形。
- `granit_window_clear_example`：Win32 窗口 acquire、清屏、submit、present 与尺寸重建循环。
- `granit_window_triangle_example`：上传位置和颜色顶点数据，在 Win32 窗口持续绘制彩色三角形。
- `granit_compute_example`：Compute Shader 写入 Storage Buffer，自动同步复制并读取结果。

示例只依赖 Granit 公共接口，不包含 Vulkan 头文件。窗口示例目前仅在 Windows 构建。
单配置生成器把动态库和可执行文件统一放入构建目录的 `bin`，可直接运行；Visual Studio 等
多配置生成器使用 `bin/Debug`、`bin/Release` 等配置子目录。

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

公共头文件按层组织：基础结果码、类型和版本位于 `granit/core`，GPU 与渲染接口位于
`granit/renderer`。根级 `granit.h` 和 `granit.hpp` 是面向普通用户的聚合入口；需要控制编译
依赖的高级用户可以直接包含分层头文件。

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
- [docs/upload-batch.md](docs/upload-batch.md)：Buffer/Texture 同步批量上传和资源保活语义。
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
- [R-07 Swapchain Backbuffer 计划](docs/plans/R-07-swapchain-backbuffer.md)。
- [V-01 生命周期验证计划](docs/plans/V-01-lifetime-validation.md)。
- [docs/plans/R-08-deferred-destruction.md](docs/plans/R-08-deferred-destruction.md)：延迟销毁计划。
- [docs/plans/R-09-render-target-attachment.md](docs/plans/R-09-render-target-attachment.md)：渲染附件计划。
- [docs/plans/F-01-command-recorder.md](docs/plans/F-01-command-recorder.md)：命令录制器计划。
- [docs/plans/F-02-command-recording.md](docs/plans/F-02-command-recording.md)：基础命令录制计划。
- [docs/plans/D-01-shader-input.md](docs/plans/D-01-shader-input.md)：Shader 输入与离线编译策略。
- [docs/plans/D-07-compute-pipeline.md](docs/plans/D-07-compute-pipeline.md)：Compute Pipeline、
  Dispatch 与资源状态计划。
- [docs/plans/D-08-pipeline-production.md](docs/plans/D-08-pipeline-production.md)：Graphics Pipeline
  完整状态、缓存、并发创建与热重载边界。
- [docs/plans/D-09-bindless-resource-table.md](docs/plans/D-09-bindless-resource-table.md)：可选
  Bindless Resource Table 的分层、回退和实施条件。
- [docs/plans/P-01-parallel-recording.md](docs/plans/P-01-parallel-recording.md)：并行录制、资源上传
  压力测试与线程安全基线。
- [docs/plans/P-02-performance-baseline.md](docs/plans/P-02-performance-baseline.md)：CPU 并发、资源
  管理与 staging 上传性能基线方案。
- [P-03 锁竞争归因与批量优化](docs/plans/P-03-contention-and-batching.md)。
- [P-04 持久化上传与 Upload Batch](docs/plans/P-04-upload-allocator.md)。
- [P-05 线程池与外部执行器边界](docs/plans/P-05-executor-boundary.md)。
- [P-06 Render Graph 职责与模块边界](docs/plans/P-06-render-graph-boundary.md)。
- [H-02 材质参数、Shader 变体与离线构建](docs/plans/H-02-material-system.md)。
- [H-02E3 持久化材质包格式](docs/plans/H-02-material-package-format.md)。
- [docs/plans/F-07-recovery-boundaries.md](docs/plans/F-07-recovery-boundaries.md)：窗口帧恢复边界计划。
- [docs/renderer.md](docs/renderer.md)：公共 renderer C/C++ API 与生命周期。
- [docs/render-target.md](docs/render-target.md)：颜色与深度/模板 Attachment 值类型。
- [docs/command-recorder.md](docs/command-recorder.md)：Command Recorder 状态与线程模型。
- [docs/resource-types.md](docs/resource-types.md)：Buffer、Texture、View 和 Sampler 值类型。
- [docs/sampler.md](docs/sampler.md)：Sampler 状态、能力限制和生命周期。
- [docs/shader.md](docs/shader.md)：SPIR-V Shader 创建、校验和生命周期。
- [docs/surface.md](docs/surface.md)：窗口 Surface、平台句柄和生命周期。
- [docs/texture.md](docs/texture.md)：Texture、Texture View 和父子生命周期。
- [docs/thread-safety.md](docs/thread-safety.md)：公开对象线程安全矩阵、提交和资源依赖约定。
- [docs/swapchain.md](docs/swapchain.md)：交换链配置、重建和生命周期。
- [docs/vulkan.md](docs/vulkan.md)：Vulkan loader、instance 和后端边界。
- [3rd/README.md](3rd/README.md)：第三方依赖版本、来源和用途。

## 许可证

Granit 使用 MIT License 发布。详细内容见 [LICENSE](LICENSE)。
