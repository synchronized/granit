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
| 六、多线程与性能 | 进行中 | P-01 并行录制、上传压力测试与线程安全矩阵已完成 |
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

**状态：已完成。前置依赖：阶段三和阶段四。**

### 目标与交付物

- **[D-01](plans/D-01-shader-input.md) / P0 / 已完成**：确定 SPIR-V 运行时输入、离线编译、
  反射边界和错误报告策略。
- **[D-02](plans/D-02-shader-module.md) / P0 / 已完成**：实现 Shader Module、输入校验、句柄
  生命周期和 C++ RAII；反射资产格式随 D-03 最小需求确定。
- **[D-03](plans/D-03-graphics-pipeline.md) / P0 / 已完成**：Graphics Pipeline、Bind Group
  Layout、不可变 Bind Group，以及 Command Recorder 的 Pipeline 和资源组绑定已经实现。
- **D-04 / P0 / 已完成**：基于 Vulkan 1.3 Dynamic Rendering 实现统一 Render Target 流程。
- **[D-05](plans/D-05-draw-commands.md) / P0 / 已完成**：实现 Viewport、Scissor、Attachment
  Clear、Vertex/Index Buffer 和基础 Draw API。
- **[D-06](plans/D-06-examples.md) / P0 / 已完成**：提供无窗口离屏清屏、窗口清屏和最小
  三角形示例。
- **[D-07](plans/D-07-compute-pipeline.md) / P1 / 已完成**：实现 Compute Pipeline、资源状态与
  Dispatch，按 D-07A 至 D-07C 分步完成。
- **[D-08](plans/D-08-pipeline-production.md) / P1 / 已完成**：完成 Graphics Pipeline 常用状态、
  Pipeline Cache、并发创建和着色器热重载边界验证。
- **[D-09](plans/D-09-bindless-resource-table.md) / P2 / 草案**：将 Bindless 定位为可选 Renderer
  Resource Table；H-02E 只预留绑定模型，待材质包完成并取得真实瓶颈数据后实现。

### 验收标准

- 示例仅使用 Granit 公共接口，不包含 Vulkan 头文件。
- 动态库调用粒度适合命令批量记录，不为每个底层状态产生不必要的 ABI 往返。
- Validation Layer 下完成常规渲染和退出，不产生生命周期或同步错误。

## 阶段六：多线程与性能

**状态：已完成。前置依赖：形成可测量的完整帧路径。**

### 目标与交付物

- **[P-01](plans/P-01-parallel-recording.md) / P1 / 已完成**：建立并行命令记录、资源创建和上传
  压力测试；独立 Buffer/Texture 上传、资源创建，以及共享只读对象的 Graphics/Compute 并行录制
  已完成。
- **[P-02](plans/P-02-performance-baseline.md) / P1 / 已完成**：已保存句柄表、Registry/资源锁、
  基础及 Graphics/Compute 混合 Recorder、Queue 提交、延迟销毁队列和 staging 上传首份基线，
  并形成 P-03/P-04 的数据依据。
- **[P-03](plans/P-03-contention-and-batching.md) / P1 / 已完成**：profiler 证明无需立即调整
  Registry 锁结构；已增加原子批量 Recorder 提交，4 线程吞吐提升约 31.9% 并显著收敛 P99。
- **[P-04](plans/P-04-upload-allocator.md) / P1 / 已完成**：已完成持久化同步上传上下文和
  Buffer/Texture Upload Batch；批量 10/100 次的单位成本显著下降，异步上传环暂缓。
- **[P-05](plans/P-05-executor-boundary.md) / P2 / 已完成**：当前不引入内部线程池或公开执行器；
  已记录外部执行器扩展点和量化重评条件。
- **[P-06](plans/P-06-render-graph-boundary.md) / P2 / 已完成**：Render Graph 确认为建立在命令和
  资源层之上的可选模块；首版串行、单队列且不做瞬态内存别名，功能实现转入 H-01。

### 验收标准

- 明确记录所有公开对象的线程安全级别。
- 不同资源上的独立操作不存在无意义的全局串行化。
- 性能调整有可复现基准数据，不以经验猜测替代测量。

## 阶段七：可选高层渲染模块

**状态：进行中。H-01～H-04 已完成，H-05 已完成至多 View 闭环。**

### 目标与交付物

- **[H-01](plans/P-06-render-graph-boundary.md#h-01-最小实现顺序) / P2 / 已完成**：H-01A～H-01E
  已完成；暂不缓存或并行化，瞬态资源池作为真实重复帧验证后的优先优化候选。
- **[H-02](plans/H-02-material-system.md) / P2 / 已完成**：H-02A 已确认材质模板、实例、参数布局、
  变体和离线工具边界，H-02B～H-02D 已完成 CPU shadow buffer、GPU 参数上传、材质 Bind Group
  及 DXC/SPIR-V 反射工具原型，H-02E1～H-02E2 已完成内存版本化材质包、稳定变体查找与
  Shader/Pipeline 缓存；H-02E3 已完成持久化容器、SHA-256、源 JSON 构建、调试导出和损坏输入
  防护；H-02F 已完成事务式迁移、热替换槽、错误材质 Pipeline 回退、可选模块目标和端到端
  示例；H-02G 已建立参数更新、变体查询、实例迁移和 Pipeline 缓存命中的性能基线。
- **[H-03](plans/H-03-pbr-renderer.md) / P2 / 已完成**：已实现离屏、前向、不透明、单方向光
  PBR 闭环，并完成 CPU/GPU 数值回归、生命周期测试和性能基线。
- **[H-04](plans/H-04-scene-submission.md) / P2 / 已完成**：已完成快照、可见性、多 View、光源
  筛选、PBR Render Graph 适配、生命周期验证和 100～10,000 对象性能基线。
- **[H-05](plans/H-05-lighting-pipeline.md) / P2 / 实现中**：H-05A～H-05D 已完成多光源、单方向光
  阴影、split-sum IBL、HDR/ACES Tone Mapping 及完整离屏像素闭环；H-05E1～H-05E8 已完成 CPU/GPU
  基线、功能降级组合、窗口 Swapchain/Resize 和多 View 完整 LDR 像素链路。当前测量尚未触发
  分块/聚簇光照。下一步依次完成 Render Graph 统一组合、多帧生命周期压力测试和跨平台/安装
  Consumer 收尾；通过后转入公共 C ABI 收敛检查点与 H-07。
- **H-06 / P2**：评估 2D、UI、调试绘制和文字渲染辅助模块。
- **[H-07](plans/H-07-reference-render-pipeline.md) / P2 / 已确认**：H-05 完成后，将 Scene、Material、
  PBR、Lighting、Post Process 和 Render Graph 组合为可选 `granit::render_pipeline` 高级参考套件；
  H-06 结果可选接入，不阻塞首版 3D 管线整合。

H-05 与 H-07 之间增加公共 API 收敛检查点：先确定 Scene、Material、Lighting 和统一渲染入口的
首版 C ABI、C++20 RAII 包装及粗粒度帧提交模型，再由 H-07 实现普通用户可直接采用的高级门面。
内部静态模块不得未经该检查点直接作为动态库 ABI 导出。

### 分层约束

- 高层模块只能依赖 Granit 核心，核心不能依赖 Scene、PBR 或 Render Graph。
- 使用者可以完全绕过高层模块，直接使用资源、命令和 Pipeline API。
- 高层模块优先使用 C++20 接口，不要求立即进入稳定 C ABI。
- 不以复刻 Filament 为目标；只吸收经过 Granit 使用场景验证的设计。
- 使用者可以只用 Renderer、选择部分高层模块或使用完整参考管线；三种模式必须并存。
- 高级参考套件定位类似 DiligentFX，但不兼容其 API、COM 对象模型或资源格式。

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
