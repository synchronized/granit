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
| 五、基础渲染 | 已完成 | Graphics/Compute Pipeline、绑定与 Draw/Dispatch 已完成 |
| 六、多线程与性能 | 已完成 | 压力测试、基线、批量提交与上传批处理已完成 |
| 七、可选高层渲染 | 已完成内部验证 | H-02～H-07 路线闭合，公共高层 ABI 后续分别设计 |
| 八、稳定化与跨平台 | 持续进行 | ABI 策略、诊断和更多平台 Surface 待后续推进 |

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
| R-10 通用资源传输 | P1 | 部分完成；mipmap 与截图便利接口按需求补充 |

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

## 五、基础渲染

**状态：已完成。**

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

**状态：进行中。**

| 任务 | 优先级 | 状态 |
|---|---:|---|
| H-01 最小 Render Graph | P2 | 已完成；详细记录见 P-06 Plan |
| [H-02 Material](plans/H-02-material-system.md) | P2 | 已完成内部原型 |
| [H-03 PBR](plans/H-03-pbr-renderer.md) | P2 | 已完成 |
| [H-04 Scene 提交](plans/H-04-scene-submission.md) | P2 | 已完成 |
| [H-05 Lighting 与后处理](plans/H-05-lighting-pipeline.md) | P2 | 已完成 |
| [H-06 Unlit、2D 与 UI](plans/H-06-unlit-2d-ui.md) | P2 | 已完成内部技术路线验证 |
| [H-07 参考 Render Pipeline](plans/H-07-reference-render-pipeline.md) | P2 | 已完成 |
| [H-08 公共 UI、Debug Draw 与 Text](plans/H-08-ui-debug-text-components.md) | P2 | 进行中；H-08A 已完成 |

高层模块只能依赖核心 Renderer，不能形成反向依赖。使用者始终可以绕过高层模块，直接使用资源、
命令和 Pipeline API。

## 八、稳定化与跨平台

**状态：持续进行。**

- **S-01 / P1**：ABI 兼容、导出符号和描述结构回归测试。
- **S-02 / P1**：日志、诊断回调、GPU 调试名称和 Device Lost 报告。
- **S-03 / P1**：安装包、真实外部 C/C++ Consumer 和版本验证。
- **S-04 / P2**：Wayland、XCB 或其他确认的平台 Surface。
- **S-05 / P2**：明确标记为不稳定的 Vulkan 原生互操作。
- **S-06 / P2**：核心、高层模块和互操作接口的兼容承诺。

## 近期执行顺序

1. 实现 H-08B：公共 Canvas Draw List 录制与参考管线提交，不泄漏内部 Material。
2. 根据测量评估透明 PBR、CSM、Clustered Forward 与 Bindless。
3. Deferred 保持为使用 Render Graph 组合的可选高级管线研究项。

若前置抽象不足，应先更新对应 Plan 和本路线图状态，再扩大公共 API。
