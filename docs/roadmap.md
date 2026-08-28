<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 路线图

本路线图只记录阶段、优先级、状态摘要和近期顺序，不构成版本或发布日期承诺。任务的设计、实施
细节和验收记录位于[开发计划](plans/README.md)。

## 优先级与状态

- **P0 核心路径**：形成首个完整、可验证渲染闭环所必需。
- **P1 完整能力**：补齐通用性、生产可用性和性能。
- **P2 扩展能力**：高层渲染、跨平台或需要真实负载验证的功能。

状态使用“已完成、进行中、待开始、暂缓”。任务编号保持稳定；设计内容以对应 Plan 为权威来源。

## 阶段总览

| 阶段 | 状态 | 结果或下一目标 |
|---|---|---|
| 一、工程与 ABI 基础 | 基本完成 | C/C++ 接口、句柄、测试和安装 Consumer 已建立 |
| 二、Vulkan 与窗口输出 | 基本完成 | Renderer、Win32 Surface、Swapchain 和帧循环已实现 |
| 三、GPU 资源 | 基本完成 | 资源、上传、回读、状态跟踪和安全退役已实现 |
| 四、命令与帧同步 | 基本完成 | Recorder、提交、Frame、查询和恢复边界已实现 |
| 五、基础渲染 | 已完成 | D-01～D-10 当前范围已完成；D-09 等待真实 Bindless 瓶颈 |
| 六、多线程与性能 | 已完成 | 压力测试、基线、批量提交与上传批处理已完成 |
| 七、可选高层渲染 | 已完成 | H-02～H-08 路线闭合，参考管线与公共 UI/Text 已验证 |
| 八、稳定化与跨平台 | 持续进行 | ABI 策略、诊断和更多平台 Surface 待后续推进 |
| 九、多后端与 Web 平台 | 进行中 | S-10A、S-10B、S-10C 已完成，下一目标为 S-10D Emscripten |
| 十、Android 移动平台 | 待开始 | 0.4.0 多后端边界完成后规划 NDK、Surface 与移动生命周期 |

## 一、工程与 ABI 基础

**状态：基本完成。**

已建立 C11 ABI、C++20 RAII、共享/静态构建、64 位安全句柄、结果码、CMake 安装导出、严格警告、
纯 C/C++ 测试和独立公共头测试。

后续工作归入稳定化阶段：日志回调、自定义分配器、导出符号回归和正式 ABI 兼容策略。

## 二、Vulkan 与窗口输出

**状态：基本完成。**

已完成 Vulkan 1.3 Loader/Instance/Device、设备筛选、独立 Volk 函数表、Win32 Surface、Swapchain
创建与重建，以及窗口 acquire/submit/present 流程。

更多平台 Surface 和可选 Vulkan 原生互操作归入稳定化阶段。

## 三、GPU 资源

**状态：基本完成。**

| 任务 | 优先级 | 状态 |
|---|---:|---|
| [R-01 GPU 内存分配](plans/R-01-memory-allocation.md) | P0 | 已完成 |
| [R-02 资源模型](plans/R-02-resource-model.md) | P0 | 已完成 |
| [R-03 Buffer](plans/R-03-buffer.md) | P0 | 已完成 |
| [R-04 Buffer 上传](plans/R-04-buffer-upload.md) | P0 | 已完成 |
| [R-05 Texture 与 View](plans/R-05-texture-view.md) | P0 | 已完成 |
| [R-06 Sampler](plans/R-06-sampler.md) | P0 | 已完成 |
| [R-07 Swapchain Backbuffer](plans/R-07-swapchain-backbuffer.md) | P0 | 已完成 |
| [V-01 生命周期验证](plans/V-01-lifetime-validation.md) | P0 | 已完成 |
| [R-08 延迟销毁](plans/R-08-deferred-destruction.md) | P0 | 已完成 |
| [R-09 Render Target Attachment](plans/R-09-render-target-attachment.md) | P0 | 已完成 |
| [R-10 通用资源传输](plans/R-10-resource-transfer.md) | P1 | 已完成；异步回读等待真实需求 |

## 四、命令与帧同步

**状态：基本完成。**

| 任务 | 优先级 | 状态 |
|---|---:|---|
| [F-01 Command Recorder](plans/F-01-command-recorder.md) | P0 | 已完成 |
| [F-02 基础命令录制](plans/F-02-command-recording.md) | P0 | 当前命令范围已完成 |
| [F-03 帧同步](plans/F-03-frame-synchronization.md) | P0 | 已完成 |
| [F-04 Queue 提交](plans/F-04-queue-submission.md) | P0 | 已完成 |
| [F-05 资源状态跟踪](plans/F-05-resource-state-tracking.md) | P0 | 当前命令范围已完成 |
| [F-06 Swapchain 帧循环](plans/F-06-swapchain-frame-loop.md) | P0 | 已完成 |
| [F-07 恢复边界](plans/F-07-recovery-boundaries.md) | P0 | 已完成 |
| F-08 多 Recorder 批量提交 | P1 | 已完成 |
| F-09 GPU 查询与标记 | P1 | Timestamp 已完成；统计与调试标记归入 S-02 |
| [F-10 公共帧上下文](plans/F-10-public-frame-context.md) | P1 | 已完成 |
| [F-11 Canvas 绑定缓存](plans/F-11-canvas-binding-cache.md) | P1 | 已完成；跨平台 CI 已通过 |
| [F-12 帧循环性能诊断与提交优化](plans/F-12-frame-loop-performance.md) | P1 | 已完成；跨平台 CI 已通过 |

## 五、基础渲染

**状态：已完成；D-01～D-10 当前范围已验收。**

| 任务 | 优先级 | 状态 |
|---|---:|---|
| [D-01 Shader 输入](plans/D-01-shader-input.md) | P0 | 已完成 |
| [D-02 Shader Module](plans/D-02-shader-module.md) | P0 | 已完成 |
| [D-03 Graphics Pipeline 与 Bind Group](plans/D-03-graphics-pipeline.md) | P0 | 已完成 |
| D-04 Dynamic Rendering | P0 | 已完成 |
| [D-05 Draw 命令](plans/D-05-draw-commands.md) | P0 | 已完成 |
| [D-06 基础示例](plans/D-06-examples.md) | P0 | 已完成 |
| [D-07 Compute Pipeline](plans/D-07-compute-pipeline.md) | P1 | 已完成 |
| [D-08 Pipeline 生产能力](plans/D-08-pipeline-production.md) | P1 | 已完成 |
| [D-09 Bindless Resource Table](plans/D-09-bindless-resource-table.md) | P2 | 草案；等待真实瓶颈 |
| [D-10 动态 Uniform Buffer Offset](plans/D-10-dynamic-uniform-buffer-offsets.md) | P1 | 已完成 |

## 六、多线程与性能

**状态：已完成。**

| 任务 | 优先级 | 状态 |
|---|---:|---|
| [P-01 并行录制与压力测试](plans/P-01-parallel-recording.md) | P1 | 已完成 |
| [P-02 性能基线](plans/P-02-performance-baseline.md) | P1 | 已完成 |
| [P-03 锁竞争与批量 API](plans/P-03-contention-and-batching.md) | P1 | 已完成 |
| [P-04 Upload Batch](plans/P-04-upload-allocator.md) | P1 | 已完成 |
| [P-05 外部执行器边界](plans/P-05-executor-boundary.md) | P2 | 已完成；当前不引入线程池 |
| [P-06 Render Graph 边界](plans/P-06-render-graph-boundary.md) | P2 | 已完成；实现转入 H-01 |

具体测量结果位于 [`benchmarks/results`](../benchmarks/results/README.md)。

## 七、可选高层渲染

**状态：已完成。**

| 任务 | 优先级 | 状态 |
|---|---:|---|
| H-01 最小 Render Graph | P2 | 已完成；详细记录见 P-06 Plan |
| [H-02 Material](plans/H-02-material-system.md) | P2 | 已完成内部原型 |
| [H-03 PBR](plans/H-03-pbr-renderer.md) | P2 | 已完成 |
| [H-04 Scene 提交](plans/H-04-scene-submission.md) | P2 | 已完成 |
| [H-05 Lighting 与后处理](plans/H-05-lighting-pipeline.md) | P2 | 已完成 |
| [H-06 Unlit、2D 与 UI](plans/H-06-unlit-2d-ui.md) | P2 | 已完成内部技术路线验证 |
| [H-07 参考 Render Pipeline](plans/H-07-reference-render-pipeline.md) | P2 | 已完成 |
| [H-08 公共 UI、Debug Draw 与 Text](plans/H-08-ui-debug-text-components.md) | P2 | 已完成 |
| [H-09 高级渲染评估](plans/H-09-advanced-rendering-evaluation.md) | P2 | 已完成；四项原型分别暂缓 |

高层模块只能依赖核心 Renderer，不能形成反向依赖。使用者始终可以绕过高层模块，直接使用资源、
命令和 Pipeline API。

## 八、稳定化与跨平台

**状态：持续进行。**

- **[S-01](plans/S-01-abi-regression.md) / P1**：已完成；导出、布局、版本扩展、共享/静态安装
  Consumer 均有回归，并已建立 Granit 0.1.0 核心 C ABI 正式快照。当前仍不承诺稳定。
- **[S-02](plans/S-02-diagnostics.md) / P1**：已完成；日志、诊断回调、GPU 调试名称和 Device Lost
  报告。
- **[S-03](plans/S-03-package-consumers.md) / P1**：实现已完成；安装包、真实外部 C/C++
  Consumer 和版本验证已落地，Windows/Linux 共享与静态 CI 矩阵已通过。
- **[S-04](plans/S-04-linux-surface.md) / P2**：实现已完成；XCB、Wayland Surface、窗口示例和
  无头集成测试均已落地，Linux GCC/Clang 共享与静态运行矩阵已通过。
- **S-05 / P2**：明确标记为不稳定的 Vulkan 原生互操作。
- **[S-06](plans/S-06-compatibility-policy.md) / P2**：当前 0.x 策略、核心 ABI 快照、component
  契约审计、变更记录和发布验收清单均已落地；最终 S-06D 等待稳定版本决策。
- **[S-07](plans/S-07-window-events.md) / P2**：可选 Window 组件及 Win32/XCB/Wayland Window、
  统一事件、原生值查询和 Renderer 集成已实现并通过跨平台 CI。
- **[S-07E](plans/S-07E-input-component.md) / P2**：Input 独立组件、C ABI、Win32 键盘文本指针及
  XCB、Wayland 键鼠文本与安装 Consumer 已实现，Linux 运行矩阵已通过。
- **[S-08](plans/S-08-third-party-integrations.md) / P2**：独立 SDL3/ImGui 目标、安装边界、SDL3
  Surface、ImGui Draw Data 转换、字体上传、组合示例与 S-08F 测量已完成；Win32 及 Linux
  X11/Wayland 共享与静态 smoke test 均已通过。
- **[S-09](plans/S-09-0.3.0-sdk-usability.md) / P1**：已完成；公共使用路径、契约一致性、诊断、
  安装 Consumer、迁移说明及 Windows/Linux 共享与静态 Release 预验证均已通过。

## 九、多后端与 Web 平台

**状态：进行中；S-10A 至 S-10D、S-10E1 至 S-10E2 已完成，准备进入 S-10E3。**

- **[S-10](plans/S-10-0.4.0-webgpu-backend.md) / P2**：先定义后端无关的内部设备、资源、命令、
  同步与 Surface 边界，在保持 Vulkan 后端功能和性能的前提下验证桌面 WebGPU 离屏 MVP；随后建立
  WGSL 工具链，并接入 Emscripten、浏览器 Canvas 和事件循环。S-10A 内部能力、插件、资源、
  Queue、Surface 与 Swapchain 边界迁移已经完成；S-10B 桌面 WebGPU 设备、资源、绑定、Pipeline、
  命令提交和确定性离屏回读已通过 Windows/Linux 真实 Dawn 验证。S-10C 已完成 WGSL 权威输入、
  诊断、反射、确定性资产与缓存闭环；S-10D 已完成 Emscripten 平台验证，S-10E1 已完成 Registry
  资源、呈现与命令路径的核心去 Vulkan 化；S-10E2 已统一异步 Provider 生命周期、状态查询、
  事件推进和终止错误传播。下一步实现 S-10E3 Canvas 与呈现。

## 十、Android 移动平台

**状态：待开始；不属于 0.4.0 交付范围。**

- **S-11 / P2**：在 S-10 多后端契约、桌面 WebGPU 和 Emscripten 路径稳定后增加 Android 支持。
  首轮以 Android NDK `arm64-v8a` 为基线，接入 `ANativeWindow`、应用暂停/恢复、Surface 重建、
  旋转与基础触控输入；同时验证 Vulkan 后端和静态接入 Dawn 的 WebGPU 后端。
- Android 交付需要独立规划 Gradle/Prefab 或 AAR 集成、按 ABI 打包、真机与模拟器测试以及移动端
  GPU 能力降级。具体版本和任务拆分在 S-10 验收后确定，不阻塞当前 0.4.0。

## 近期执行顺序

1. 实现 S-10E3 Canvas Surface、借用 Backbuffer、Acquire/Present 和 Frame 令牌校验。
2. 依次完成 S-10E4 公共 Renderer 接入及 S-10E5 示例与验收。
3. S-06D 最终验收等待稳定版本与 component 范围决策；不在 0.x 阶段提前宣布稳定。
4. 完成 0.4.0 多后端与 Web 路径后，为 S-11 建立独立 Android Plan，再进入 NDK 与移动 Surface
   实现。
5. H-09 的透明 PBR、CSM、Clustered Forward 与 Bindless 只在各自重新评估条件满足后独立恢复，
   不作为当前稳定化工作的前置项。

若前置抽象不足，应先更新对应 Plan 和本路线图状态，再扩大公共 API。
