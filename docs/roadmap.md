<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 路线图

本路线图只记录当前阶段、正在实施的任务、暂缓候选和长期方向，不构成版本或发布日期承诺。
已发布版本的变化见 [Changelog](../CHANGELOG.md)，详细设计见[计划索引](plans/README.md)，实际执行
证据见[实施记录](records/README.md)。

## 当前阶段

Granit 已形成桌面 Vulkan、浏览器 WebGPU、C11 ABI、C++20 RAII、Window、RenderPipeline 与
AssetTools 的完整闭环。当前仍处于 0.x，v0.55.0 已发布。Bindless 继续作为 Vulkan 内部实验，
下一阶段不直接冻结公共 Bindless API。
不改变 Vulkan 驱动由平台提供的边界。

| 能力域 | 状态 | 当前边界 |
|---|---|---|
| Core 与 ABI | 可用，未冻结 | 结果码、强类型句柄、C/C++ 包装、诊断与安装 Consumer 已覆盖 |
| Vulkan 桌面后端 | 可用 | Windows Win32 与 Linux XCB/Wayland，资源、命令、提交和呈现完整 |
| WebGPU 浏览器后端 | 可用 | Emscripten 浏览器资源、PBR、异步操作、Timestamp 与呈现闭环 |
| GPU 资源与 Pipeline | 可用 | Buffer、Texture、Sampler、Shader Library、Graphics/Compute 与传输 |
| Frame 与 Swapchain | 可用 | Frame Context、在途槽、恢复边界、取消、Present 与延迟回收 |
| RenderPipeline | 可用，未冻结 | Scene Snapshot、Material、Forward PBR、Shadow、IBL、UI 与后处理 |
| Window 与 Input | 可用，未冻结 | 单一 Window System 管理窗口、事件、输入状态和直接 Surface 创建 |
| AssetTools | 实验性 | HLSL-first Shader Library、Material、Texture、Environment 构建与检查 |
| SDK 与发布 | 可用 | Windows/Linux shared SDK、静态源码构建、安装审计与不可变候选晋级 |

各能力的准确使用方式和限制以 [Reference](README.md#api-与行为参考) 为准。

## 当前计划

[v0.55.0](versions/v0.55.0.md) 已由
[S-91](plans/S-91-0.55.0-bindless-pressure-and-boundary.md) 完成：保留 Vulkan 内部 Bindless
实验、传统 Bind Group 默认路径和 WebGPU 回退，不新增公共 Bindless ABI。

[v0.54.0](versions/v0.54.0.md) 已由
[S-90](plans/S-90-0.54.0-vulkan-bindless-admission.md) 完成：验证内部 Vulkan Descriptor Indexing、
生命周期契约和传统 Bind Group 对照，WebGPU 继续回退；公共 Bindless ABI 不进入本版本。

[v0.52.0](versions/v0.52.0.md) 已由
[S-88](plans/S-88-0.52.0-binding-pressure-and-resource-table-gate.md) 承接：补齐真实 Shader 采样
绑定压力证据，并决定暂不进入 Resource Table 原型。v0.52.0 不默认启用 Bindless，也不承接
Gneiss 的 SVT/纹理驻留职责。后续候选需由新的真实负载证据驱动。

### v0.52.0：真实绑定压力与 Resource Table 闸门

**状态：已发布。**

[v0.52.0](versions/v0.52.0.md) 通过 [S-88](plans/S-88-0.52.0-binding-pressure-and-resource-table-gate.md)
完成真实 `pbr_textured` 采样压力基线。8/64/512 材质规模数据尚不足以证明 Resource Table 或
Bindless 是主要瓶颈，因此传统 Bind Group 保持默认路径；最终证据见
[S-88 记录](records/2026-10-07-s88-binding-pressure-baseline.md)。

## 最近完成

### v0.54.0：Vulkan Bindless 准入验证

**状态：已发布。**

完成内部 sampled texture/sampler Descriptor Indexing、显式 Shader/Pipeline 变体、GPU 完成点后的
安全回收和传统 Bind Group 对照。当前数据未证明 Bindless 有稳定收益，因此传统 Bind Group 仍是
默认路径；公共 Bindless ABI 和 Linux/Lavapipe 大规模端到端对照留待后续版本。

### v0.55.0：Bindless 真实压力与边界复评

**状态：已发布。**

完成 Descriptor/CPU 压力、GPU timestamp、生命周期、容量耗尽和平台回退验证；当前数据未证明
Bindless 有跨平台稳定收益，因此传统 Bind Group 继续默认，公共 Bindless ABI 不进入后续 API。

### v0.53.0：Bindless 能力探针与 Resource Table 原型

**状态：已发布。**

[v0.53.0](versions/v0.53.0.md) 通过 [S-89](plans/S-89-0.53.0-bindless-probe-and-resource-table.md)
完成 CPU Resource Table、后端无关能力位和 `12_bindless_probe`。真实 Vulkan Descriptor 数组采样
延期，等待资源索引写入、Shader/Pipeline 变体和 GPU 生命周期契约稳定；传统 Bind Group 仍是默认路径。

### v0.51.0：RenderPipeline 契约与真实 Consumer

**状态：已发布。**

[v0.51.0](versions/v0.51.0.md) 通过
[S-87](plans/S-87-0.51.0-render-pipeline-contract-and-consumer.md) 完成 Consumer、生命周期和
跨后端编译验收。

### v0.50.0：公共 API/ABI 稳定候选审计

**状态：已发布。**

[v0.50.0](versions/v0.50.0.md) 通过
[S-86](plans/S-86-0.50.0-api-abi-stability-candidate.md) 完成 component 分级、兼容策略、ABI
门禁和大型功能恢复条件复评。

### v0.49.0：异步回读契约与诊断

**状态：已发布。**

[v0.49.0](versions/v0.49.0.md) 通过
[S-85](plans/S-85-0.49.0-readback-contract-and-diagnostics.md) 修复非零结果的空回读缓冲区边界，
补充 Readback Batch Reference，并完成 Windows/Linux/Emscripten 回归。

### v0.48.0：Runtime Consumer 与图形能力验证

**状态：已发布。**

[v0.48.0](versions/v0.48.0.md) 通过
[S-84](plans/S-84-0.48.0-runtime-consumer-and-capability-verification.md) 完成 Runtime Bundle 的真实
Consumer 接入、跨平台部署诊断和现有 Tutorial 能力验收。

### v0.47.0：Vulkan Runtime Bundle 与验证层交付

**状态：已发布。**

[v0.47.0](versions/v0.47.0.md) 通过 [S-83](plans/S-83-0.47.0-vulkan-runtime-bundle.md) 交付
Windows/Linux Runtime Bundle、system/bundled/auto Loader 选择、Validation Layer 配置以及公开
制品的 manifest、许可证和 SHA-256 验收。

### v0.45.0：教程能力矩阵与图形特性探针

**状态：已发布，P1。**

[v0.45.0](versions/v0.45.0.md) 通过
[S-82](plans/S-82-0.45.0-tutorial-capability-matrix.md) 参考 bgfx 单项示例，新增粒子、透明和
能力查询探针，验证现有公共 API 并分类真实缺口。Gneiss `UPSTREAM-044` 的设备能力快照、变体
选择、CPU 负载生命周期和预算回收仍由 Gneiss 负责。

### v0.44.0：独立纹理变体上传

**状态：已发布。**

[v0.44.0](versions/v0.44.0.md) 通过 [S-81](plans/S-81-0.44.0-texture-variant-upload.md) 承接 Gneiss
`UPSTREAM-044` 的 U44-01，补齐非零偏移纹理变体的局部负载上传。预算、驻留、VFS、任务调度和
RID 生命周期仍由 Gneiss 负责。

### v0.43.0：Gneiss PBR 兼容性审计

**状态：已发布，P1。**

[v0.43.0](versions/v0.43.0.md) 通过 [S-80](plans/S-80-0.43.0-gneiss-pbr-compatibility.md) 核对 Gneiss
`UPSTREAM-043` 与 Granit v0.42.0 已发布的五项 PBR 契约。默认不新增公共 API；只有确认属于 Granit 的
通用缺口才实施最小修复。

### v0.42.0：Model Viewer 架构与跨平台收敛

**状态：已发布。**

[v0.42.0](versions/v0.42.0.md) 通过
[S-77](plans/S-77-0.42.0-model-viewer-convergence.md) 重新划分 glTF Rendering、Asset Manager、
任务执行、Viewer 状态和渲染职责，再由
[S-78](plans/S-78-0.42.0-model-viewer-platform-reliability.md) 统一 Desktop/Web 的 ImGui 加载流程，
修复 v0.41.0 遗留的 Linux 托管 Emscripten 浏览器 WebGPU 生命周期波动，并完成原生 Linux 验收；
[S-79](plans/S-79-0.42.0-imgui-integration-layout.md) 将已重叠的 ImGui 综合 Sample 收敛为
`08_sdl_imgui` 第三方宿主集成教程、Integration 测试、Web 验收和 Benchmark。

### v0.41.0：标准 PBR 材质正确性

**状态：已发布。**

[v0.41.0](versions/v0.41.0.md) 完成 S-76，修复反射实例的切线空间与正面判定，并补齐标准 PBR
的 OPAQUE/MASK/BLEND、双面、逐贴图 UV/Sampler 和顶点色契约；最终结果见
[S-76 验收记录](records/2026-09-29-s76-pbr-material-correctness-acceptance.md)。

### v0.40.0：Deferred 与多 Pass 渲染验证

**状态：已发布。**

[v0.40.0](versions/v0.40.0.md) 完成 S-75，以 `07_deferred` 验证跨后端 MRT G-buffer、全屏光照、
调试视图与 Resize 工作流。现有公共 API 足以表达该流程，本版本没有增加 Render Graph 或
Transient Texture；最终结果见
[S-75 验收记录](records/2026-09-29-s75-deferred-rendering-acceptance.md)。

### v0.39.0：统一示例基础与动态 GPU 工作流

**状态：已发布。**

[v0.39.0](versions/v0.39.0.md) 完成 S-72～S-74，统一示例资产、任务和交互基础，补齐公共 Indirect、
Frame Transient Buffer 与 GPU Marching Cubes Tutorial。最终范围和验收证据由版本文档统一索引。

### S-71：0.38.0 跨后端能力契约与 WebGPU 补齐

**状态：已完成，P1。**

[S-71](plans/S-71-0.38.0-webgpu-capability-parity.md) 已建立 Vulkan 与浏览器 WebGPU 的正式能力
矩阵，补齐 Primitive Topology、MRT、可准确映射的格式与 Write-Only Storage Texture，并把无法
可靠映射的能力固定为明确的不支持结果。版本已发布为 `v0.38.0`。

### S-70：0.37.0 特性教程与 API 能力验证

**状态：已完成，P1。**

[S-70](plans/S-70-0.37.0-feature-tutorials.md) 已交付 Instancing、Raymarch 与 Metaballs 三个独立
教程，让桌面 Vulkan 与浏览器 WebGPU 编译同一主体代码，并通过自动 Smoke、浏览器验收和能力
审计确认现有 API 足以表达本轮效果。Indirect Draw/Dispatch 留作后续独立候选，版本已发布为
`v0.37.0`。

### S-69：0.36.0 公共 API 稳定边界与最小数学库

**状态：已完成，P1。**

[S-69](plans/S-69-0.36.0-api-stability-and-math.md) 已逐公共头确定长期保留候选、观察期、实验性
与内部范围，公开统一坐标约定的最小 C++20 数学接口，收敛 Renderer、Model Viewer 与 Tutorial
使用路径，并发布 `v0.36.0`。本版本仍未冻结整个 ABI。

### S-68：0.35.0 稳定候选契约审计

**状态：已完成，P1。**

[S-68](plans/S-68-0.35.0-stable-candidate-contracts.md) 已为 Core、RenderPipeline 与 Window 建立
component 级布局和符号快照、负向行为契约、安装 SDK Consumer 与统一证据参考，并发布
`v0.35.0`。这些证据用于后续稳定决策，本版本仍未冻结 ABI。

### S-67：0.34.0 公共 SDK 与 AssetTools 契约收敛

**状态：已完成，P1。**

[S-67](plans/S-67-0.34.0-sdk-contract-convergence.md) 已统一四类 AssetTools 结果查询，建立稳定候选
component 的跨版本 ABI 门禁，收口普通 C++ 用户路径与安装 SDK 验收，并发布 `v0.34.0`。

### S-66：0.33.0 AssetTools 诊断与 CI 收敛

**状态：已完成，P1。**

[S-66](plans/S-66-0.33.0-asset-tools-ci-convergence.md) 已为 Shader Library 源构建补齐结构化诊断，
统一锁定 Shader 工具链的 CI 入口和安装 SDK CMake helper，使 Pull Request 门禁与变更风险匹配，
并发布 `v0.33.0`。

### S-65：0.32.0 安装 SDK 图形工作流

**状态：已完成，P1。**

[S-65](plans/S-65-0.32.0-sdk-graphics-workflow.md) 已加固版本准备脚本，补齐安装后 SDK 从 HLSL
Shader Library 到首个 Draw 的独立消费闭环，并发布 `v0.32.0`。

### S-64：0.31.0 SDK-first 示例与资产入口

**状态：已完成，P1。**

[S-64](plans/S-64-0.31.0-sdk-first-examples.md) 已增加只依赖安装后 CMake package 的 Window
Quickstart，将其接入 shared SDK 端到端验收，并明确 Tutorials、Samples、Example Common 与统一
资产入口的边界；本版本没有新增公共 VFS 或渲染特性。

### S-53～S-63：0.30.0 文档、教程、Window、资产与 Model Viewer 收敛

**状态：已完成，P1。**

[S-53](plans/S-53-0.30.0-documentation-tutorial-release.md) 至
[S-63](plans/S-63-model-viewer-session-platform-shells.md) 已交付 Cube 与 PBR Assets 特性教程、
Window Target/SDL3、统一 Example Asset System，以及 Desktop/Web 共用的 Model Viewer Application、
ImGui、Render Service、Render Runtime 和任务执行语义；Frame 生命周期文档和 shared-only Release
也已完成。完整远端跨平台矩阵和公开产物复验已经通过，并发布 `v0.30.0`。

### S-52：0.29.0 版本收口与发布验收

**状态：已完成，P1。**

[S-52](plans/S-52-0.29.0-release-acceptance.md) 已完成迁移说明、跨平台测试、四套 SDK 候选包、
SHA-256 校验与公开下载复验，并发布 `v0.29.0`。

### S-51：0.29.0 Shader Library 逻辑名称

**状态：已完成，P1。**

[S-51](plans/S-51-0.29.0-shader-library-logical-names.md) 已把 Library 与 Shader 逻辑名称写入
`.grshlib`，运行时和 Material Builder 直接消费该归档，并删除独立索引和生成内容 ID include。

### S-50：0.29.0 Model Viewer 教程迁移

**状态：已完成，P1。**

[S-50](plans/S-50-0.29.0-model-viewer-tutorial-migration.md) 已新增 09 Model Loading，并把跨后端
Model Viewer 收敛为 Tutorial 10。桌面、Web 与离屏入口已经切换，重复 Sample 已删除。

### S-48：线性入门教程与配套示例

**状态：已完成，P1。**

[S-48](plans/S-48-0.28.0-linear-tutorial-series.md) 已交付从 Window、Triangle、Texture、Camera、
Mesh、Material/Lighting、Render Pipeline 到 ImGui 的八章连续教程。全部章节具有配套源码与桌面
自动验证，Window 与 ImGui 章节同时覆盖浏览器 WebGPU；普通 C++ 使用路径已完成强类型审计。

### S-49：0.28.0 Window 跨平台可选主循环

**状态：已完成，P1。**

[S-49](plans/S-49-0.28.0-window-loop.md) 已在非阻塞 Window 事件泵之上增加可选托管 Loop。桌面和
Emscripten 可复用同一个应用 Tick，Loop 不持有或推进 Renderer，也不接管资源或引擎任务系统。

### S-47：Window 跨平台入口收敛

**状态：已完成，P1。**

[S-47](plans/S-47-window-platform-convergence.md) 已让 Emscripten 与桌面平台共享 Window System、
窗口和输入事件、状态查询及 Surface 创建流程。浏览器安装包现在导出 Window component，Model
Viewer 不再维护私有 DOM 输入和 Canvas Surface 生命周期。

### S-45：0.26.0 文档一致性与历史收敛

**状态：已完成，P1。**

[S-45](plans/S-45-0.26.0-documentation-convergence.md) 已修正当前文档与 0.25.0 实现之间的事实
漂移，收敛 Concept、Reference、Roadmap、Plan 与 Record 职责，并扩展确定性文档检查。本任务未
修改公共 API、ABI、资产格式或运行时行为。

完成内容：

1. 修正版本、组件和发布状态。
2. 让 Concept 与 Reference 反映当前后端和资源能力。
3. 压缩路线图、计划索引和长实施记录。
4. 增加版本身份与已删除 component 检查，完成 Documentation 验收。

## 暂缓与重新评估条件

以下方向没有进入当前版本。只有满足对应证据后才建立新的实施计划。

| 方向 | 当前决定 | 恢复条件 |
|---|---|---|
| [Bindless Resource Table](plans/D-09-bindless-resource-table.md) | 暂缓 | 真实材质绑定压力证明传统 Bind Group 成为瓶颈 |
| [透明 PBR](plans/H-09B-transparent-pbr-correctness.md) | 已恢复 | 真实 glTF 场景已经提供 Alpha、排序和验收需求，由 S-76 实施 |
| Clustered Forward | 暂缓 | 多光源负载稳定超过当前 Forward 路径预算 |
| Cascaded Shadow Maps | 暂缓 | 大尺度室外场景证明单方向光 Shadow Map 不足 |
| 公共 glTF/Scene SDK | 暂缓 | 至少第二个独立 Consumer 需要复用示例私有加载器 |
| 公共执行器或线程池 | 暂缓 | 至少第二个模块需要相同调度与取消契约 |
| Android | 待规划 | 明确 NDK、Surface、生命周期、输入和 CI 设备矩阵 |
| 稳定 ABI | 待决策 | 稳定 component 范围和兼容门槛全部满足 |

这些方向互不构成前置依赖。测量或 Consumer 证据不足时继续使用当前实现，不提前扩展公共 ABI。

## 长期方向

- 保持 Renderer、RenderPipeline、Window、AssetTools 与第三方 Integration 的单向依赖。
- 继续以 Vulkan 和浏览器 WebGPU 的共同语义为公共契约，不为表面对称模拟不安全能力。
- 用格式版本管理持久化资产兼容，用 component 分别声明 API/ABI 稳定等级。
- 优先改进真实上游接入、诊断、性能测量和发布复现，再增加渲染特性。
- 原生后端互操作若出现明确需求，作为显式不稳定高级接口设计，不污染基础 API。

## 历史入口

- [Changelog](../CHANGELOG.md)：面向使用者的逐版本变化和迁移影响。
- [迁移指南](guides/migrate-0.28-to-0.29.md)：最近一次破坏性版本迁移。
- [计划索引](plans/README.md)：当前与暂缓任务；[完成计划索引](plans/completed.md)保存已验收计划。
- [实施记录](records/README.md)：跨平台、性能和发布验收证据。

扩大公共 API 前应先更新对应 Plan 和本路线图状态，并明确验证与退出条件。
