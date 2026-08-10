<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 路线图

路线图用于约束实现顺序和阶段边界，不构成版本或发布日期承诺。项目进入稳定版本前，公共 API
和 ABI 仍可根据实现验证调整；每次调整都必须同步 C API、C++ 包装、测试和文档。

## 组织与优先级

路线图采用“阶段 + 阶段内优先级”，不使用单一扁平清单：

- **P0 核心路径**：形成第一个完整、可验证渲染闭环所必需；默认按依赖顺序执行。
- **P1 完整能力**：核心闭环完成后补齐通用性、生产可用性和性能。
- **P2 扩展能力**：高层渲染、跨平台、高级互操作或需要真实负载验证的功能。

任务状态使用“已完成、进行中、待开始、暂缓”。编号一旦写入文档尽量保持稳定，方便提交和问题
跟踪引用；优先级可以随验证结果调整，但调整时必须说明依赖变化。

## 当前进度

| 阶段 | 状态 | 说明 |
| --- | --- | --- |
| 一、工程与 ABI 基础 | 基本完成 | 构建、公共接口分层、句柄、测试体系已建立 |
| 二、Vulkan 与窗口输出基础 | 基本完成 | Renderer、Win32 Surface、Swapchain 生命周期已实现 |
| 三、GPU 资源基础 | 进行中 | 内存分配与资源值类型已完成，资源生命周期尚未实现 |
| 四、命令与帧同步 | 进行中 | Command Recorder、基础命令和内部帧同步已实现 |
| 五、基础渲染 | 进行中 | Shader Module 已实现，Pipeline 与绘制接口尚未实现 |
| 六、多线程与性能 | 未开始 | 在真实访问模式形成后细化 |
| 七、可选高层渲染模块 | 暂缓 | 不阻塞核心库完成，保持独立分层 |
| 八、稳定化与跨平台 | 持续进行 | Linux 窗口系统和 ABI 稳定策略留待后续 |

## 阶段一：工程与 ABI 基础

**状态：基本完成。**

### 已交付

- C11 ABI、C++20 RAII 包装、共享库导出和可选静态构建。
- 64 位整数句柄，以及类型、generation 和 Renderer domain 校验。
- 结果码、错误文本、描述结构 `struct_size` 兼容规则。
- CMake preset、安装包导出、严格警告和格式配置。
- Unity 纯 C API 测试、Catch2 C++/内部测试和独立公共头编译测试。

### 后续补充

- 日志回调和自定义分配器接口。
- 安装后 C/C++ consumer 自动化测试。
- 公共 API/ABI 版本和兼容策略。

### 验收标准

- 动态库和静态库均可由 C 与 C++ consumer 使用。
- 公共头文件不包含 Vulkan 或平台 SDK 头文件。
- 动态库只导出明确声明的 C ABI 符号。

## 阶段二：Vulkan 与窗口输出基础

**状态：基本完成。**

### 已交付

- Vulkan Loader、Vulkan 1.3 Instance、物理设备筛选、逻辑设备和 graphics queue。
- 独立 Instance/Device Volk 函数表及后端错误映射。
- Win32 Surface，以及平台扩展和呈现能力的按需启用。
- Swapchain 创建、查询、原子重建、呈现模式回退和级联销毁。
- Registry 共享所有权和 Renderer 局部资源锁；Vulkan 创建和销毁不占用 Registry 锁。

### 暂不包含

- Swapchain 图像获取和呈现。
- Linux、macOS 或移动平台 Surface。
- 面向普通用户的 Vulkan 原生互操作。

### 验收标准

- 能在真实 Win32 窗口创建、重建并销毁 Swapchain。
- 错误 Renderer domain、资源类型和失效句柄均被拒绝。
- 销毁 Renderer 或 Surface 时按正确顺序清理所有子对象。

## 阶段三：GPU 资源基础

**状态：进行中。前置依赖：阶段一和阶段二。**

### 目标与交付物

1. **[R-01](plans/R-01-memory-allocation.md) / P0 / 已完成**：采用内部 VMA 的 GPU 内存分配方案。
2. **[R-02](plans/R-02-resource-model.md) / P0 / 已完成**：定义平台无关的资源用途、内存位置、像素格式、尺寸和采样数。
3. **[R-03](plans/R-03-buffer.md) / P0 / 已完成**：实现 Buffer 创建、销毁、映射、刷新和失效语义。
4. **[R-04](plans/R-04-buffer-upload.md) / P0 / 已完成**：实现初始数据和同步 device-local Buffer 上传路径。
5. **[R-05](plans/R-05-texture-view.md) / P0 / 已完成**：实现 Texture 与 Texture View 生命周期、用途和子资源范围。
6. **[R-06](plans/R-06-sampler.md) / P0 / 已完成**：实现 Sampler 描述、能力限制和缓存策略。
7. **[R-07](plans/R-07-swapchain-backbuffer.md) / P0 / 已完成**：将 Swapchain 图像接入为非拥有 Texture/View。
8. **[V-01](plans/V-01-lifetime-validation.md) / P0 / 已完成**：增加 Granit 资源生命周期验证与销毁诊断。
9. **[R-08](plans/R-08-deferred-destruction.md) / P0 / 已完成**：普通 GPU 资源接入真实提交完成点，
   Swapchain 重建和销毁具备可靠的 Queue Present 空闲边界。
10. **[R-09](plans/R-09-render-target-attachment.md) / P0 / 已完成**：定义统一
    Render Target Attachment，覆盖颜色和深度/模板附件。
11. **R-10 / P1**：补充纹理上传、mipmap、复制、读取回 CPU 和截图能力。

### 设计约束

- 公共 API 不出现 `VkBuffer`、`VkImage`、Vulkan usage flag 或内存类型索引。
- 描述结构使用 Granit 自己的定宽枚举和位标志。
- 同一 Renderer 的资源可从不同线程创建；同一资源的写操作必须定义同步要求。
- 高频上传不能设计成每小段数据一次动态库调用，应支持批量或上传上下文。

### 验收标准

- 覆盖创建、映射、上传、错误 domain、重复销毁和父对象级联销毁。
- 至少验证 host-visible Buffer 写入及 device-local Buffer 上传。
- Texture 格式和用途经过能力检查，错误组合返回稳定的 Granit 结果码。

### 暂不包含

- Shader、Pipeline 和渲染命令。
- 面向使用者的线程池或任务系统。

## 阶段四：命令与帧同步

**状态：进行中。前置依赖：阶段三的 Buffer、Texture View 和延迟销毁基础。**

### 目标与交付物

- **[F-01](plans/F-01-command-recorder.md) / P0 / 已完成**：实现独占 Command Pool 和可跨线程
  移交的 Command Recorder。
- **[F-02](plans/F-02-command-recording.md) / P0 / 基础命令已完成**：实现 Buffer Copy/Fill 和
  Dynamic Rendering；资源屏障随 F-05 接入。
- **[F-03](plans/F-03-frame-synchronization.md) / P0 / 已完成**：实现 Fence、Semaphore 及每帧
  上下文的内部抽象。
- **[F-04](plans/F-04-queue-submission.md) / P0 / 已完成**：实现可配置 frames-in-flight 和
  Queue 提交串行化。
- **[F-05](plans/F-05-resource-state-tracking.md) / P0 / 当前命令范围已完成**：建立资源状态跟踪
  和必要的同步信息记录；后续用途随对应命令扩展。
- **[F-06](plans/F-06-swapchain-frame-loop.md) / P0 / 已完成**：实现 Swapchain acquire、提交、
  present 和 out-of-date 重建流程。
- **[F-07](plans/F-07-recovery-boundaries.md) / P0 / 已完成**：实现 Frame 回收、窗口零尺寸、
  Surface Lost 与 Renderer 全局 Device Lost 终止状态。
- **F-08 / P1**：支持多个线程使用独立 Recorder 并合并到同一帧提交。
- **F-09 / P1**：增加时间戳、统计查询和 GPU 调试标记。

### 线程模型

- 不同 Command Recorder 可以在不同线程并行记录。
- Vulkan Queue 的外部同步由 Granit 内部保证。
- Registry 锁只管理身份和所有权，不承担 GPU 同步。
- Renderer/Surface/Swapchain 销毁不得与对应资源操作并发；如需放宽，必须增加明确的关闭状态。

### 验收标准

- 多帧循环可以稳定 acquire、提交和 present。
- resize、最小化和恢复不会泄漏资源或使用旧 Swapchain 图像。
- 同一 Renderer 的多线程命令记录不依赖一把全局大锁。

## 阶段五：基础渲染

**状态：未开始。前置依赖：阶段三和阶段四。**

### 目标与交付物

- **[D-01](plans/D-01-shader-input.md) / P0 / 已完成**：确定 SPIR-V 运行时输入、离线编译、
  反射边界和错误报告策略。
- **[D-02](plans/D-02-shader-module.md) / P0 / 已完成**：实现 Shader Module、输入校验、句柄
  生命周期和 C++ RAII；反射资产格式随 D-03 最小需求确定。
- **[D-03](plans/D-03-graphics-pipeline.md) / P0 / 已完成**：Graphics Pipeline、Bind Group
  Layout、不可变 Bind Group，以及 Command Recorder 的 Pipeline 和资源组绑定已经实现。
- **D-04 / P0**：基于 Vulkan 1.3 Dynamic Rendering 实现统一 Render Target 流程。
- **D-05 / P0**：实现 Viewport、Scissor、Clear、Vertex/Index Buffer 和基础 Draw API。
- **D-06 / P0**：提供无窗口离屏清屏、窗口清屏和最小三角形示例。
- **D-07 / P1**：实现 Compute Pipeline 与 Dispatch。
- **D-08 / P1**：补充 Pipeline Cache、异步创建和着色器热重载边界。

### 验收标准

- 示例仅使用 Granit 公共接口，不包含 Vulkan 头文件。
- 动态库调用粒度适合命令批量记录，不为每个底层状态产生不必要的 ABI 往返。
- Validation Layer 下完成常规渲染和退出，不产生生命周期或同步错误。

## 阶段六：多线程与性能

**状态：未开始。前置依赖：形成可测量的完整帧路径。**

### 目标与交付物

- **P-01 / P1**：建立并行命令记录、资源创建和上传压力测试。
- **P-02 / P1**：测量句柄表、资源锁、Queue 锁和延迟销毁队列。
- **P-03 / P1**：依据数据调整锁粒度、缓存、暂存内存和批量 API。
- **P-04 / P1**：建立瞬态资源与每帧上传分配器。
- **P-05 / P2**：评估内部线程池；默认不要求使用者采用 Granit 的任务系统。
- **P-06 / P2**：评估 Render Graph，并优先作为建立在命令和资源层之上的独立模块。

### 验收标准

- 明确记录所有公开对象的线程安全级别。
- 不同资源上的独立操作不存在无意义的全局串行化。
- 性能调整有可复现基准数据，不以经验猜测替代测量。

## 阶段七：可选高层渲染模块

**状态：暂缓。前置依赖：核心资源、命令和基础渲染闭环稳定。**

### 目标与交付物

- **H-01 / P2**：实现独立 Render Graph，负责 Pass 依赖、裁剪和瞬态资源生命周期。
- **H-02 / P2**：建立材质参数、着色器变体和离线材质构建流程。
- **H-03 / P2**：提供可选的金属度/粗糙度 PBR 渲染模块。
- **H-04 / P2**：提供场景提交、Camera、Light 和可见性输入适配层。
- **H-05 / P2**：实现阴影、环境光照、后处理和色调映射的参考管线。
- **H-06 / P2**：评估 2D、UI、调试绘制和文字渲染辅助模块。

### 分层约束

- 高层模块只能依赖 Granit 核心，核心不能依赖 Scene、PBR 或 Render Graph。
- 使用者可以完全绕过高层模块，直接使用资源、命令和 Pipeline API。
- 高层模块优先使用 C++20 接口，不要求立即进入稳定 C ABI。
- 不以复刻 Filament 为目标；只吸收经过 Granit 使用场景验证的设计。

### 验收标准

- 核心库和高层模块可以独立构建、测试和发布。
- 离屏、窗口和多 View 场景使用同一资源与 Render Target 模型。
- 材质或场景接口不会暴露 Vulkan 类型，也不会限制底层显式命令能力。

## 阶段八：稳定化与跨平台

**状态：持续进行。**

### 目标与交付物

- **S-01 / P1**：增加 ABI 兼容、导出符号和旧描述结构自动化回归测试。
- **S-02 / P1**：实现日志、诊断回调、GPU 调试名称和 Device Lost 报告。
- **S-03 / P1**：完成安装包、真实外部 C/C++ consumer 和版本兼容验证。
- **S-04 / P2**：实现 Wayland、XCB 或其他经过确认的平台 Surface。
- **S-05 / P2**：评估明确标记为不稳定的高级 Vulkan 互操作接口。
- **S-06 / P2**：明确核心、高层模块和高级互操作接口各自的兼容承诺。

### 验收标准

- 支持的平台拥有构建、运行和窗口重建测试。
- 普通用户路径始终不依赖 Vulkan SDK。
- 稳定版本发布前形成书面的 API/ABI 兼容承诺。

## 近期执行顺序

1. `D-03` 至 `D-06`：完成 Pipeline、离屏清屏和最小三角形。

若实现过程中发现前置抽象不足，应先更新本路线图和对应设计文档，再扩大公共 API。
