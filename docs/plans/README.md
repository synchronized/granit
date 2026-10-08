<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 开发计划文档

本目录保存单项路线图任务的目标、设计、步骤和验收条件。计划不代表已经实现的公共能力；当前
行为以 Reference、Concept 和仓库实现为准。全局优先级见[路线图](../roadmap.md)，执行结果见
[实施记录](../records/README.md)。

## 当前计划

- [S-93：0.57.0 诊断硬化与跨后端回归](S-93-0.57.0-diagnostics-hardening-and-regression.md)——固化 Frame Trace
  语义、后端命令回归、统一 CI 诊断附件和 RenderDoc 手动验收，不新增公共 ABI。

- [S-92：0.56.0 帧诊断与 RenderDoc 协作](S-92-0.56.0-frame-diagnostics-and-renderdoc.md)——增加可选 RenderDoc
  单帧捕获、Vulkan 标记和跨后端 Frame Trace，不引入 RenderDoc/Vulkan 公共依赖。

- [S-88：0.52.0 真实绑定压力与 Resource Table 闸门](S-88-0.52.0-binding-pressure-and-resource-table-gate.md)——
  补齐真实 Shader 采样压力证据，再决定是否进入 Resource Table 原型。

## 最近完成

- [S-91：0.55.0 Bindless 真实压力与边界复评](S-91-0.55.0-bindless-pressure-and-boundary.md)——完成
  Descriptor/CPU 压力、GPU 时间戳、容量耗尽和跨平台回退证据，并发布 `v0.55.0`；不新增公共 Bindless ABI。

- [S-90：0.54.0 Vulkan Bindless 准入验证](S-90-0.54.0-vulkan-bindless-admission.md)——完成内部 Descriptor Indexing、
  资源生命周期、显式 Shader/Pipeline 变体和传统 Bind Group 对照，保持公共 API 不变，并发布 `v0.54.0`。

- [S-89：0.53.0 Bindless 能力探针与 Resource Table 原型](S-89-0.53.0-bindless-probe-and-resource-table.md)——完成 CPU
  Resource Table、能力探针和传统 Bind Group 回退，并发布 `v0.53.0`；真实 Vulkan Descriptor 数组采样延期。

- [S-87：0.51.0 RenderPipeline 契约与真实 Consumer](S-87-0.51.0-render-pipeline-contract-and-consumer.md)——完成
  Material、Scene、Canvas、Text 和 Debug Draw 的真实复用边界与跨后端生命周期验收，并发布 `v0.51.0`。

- [S-86：0.50.0 公共 API/ABI 稳定候选审计](S-86-0.50.0-api-abi-stability-candidate.md)——完成 component
  稳定等级、布局/符号、Consumer、兼容策略和大型功能恢复条件复评，并发布 `v0.50.0`。

- [S-85：0.49.0 异步回读契约与诊断](S-85-0.49.0-readback-contract-and-diagnostics.md)——修复非零结果的
  空回读缓冲区边界，补齐 Readback Batch Reference，并完成 Windows/Linux/Emscripten 验收。

- [S-84：0.48.0 Runtime Consumer 与图形能力验证](S-84-0.48.0-runtime-consumer-and-capability-verification.md)——
  完成 Runtime Bundle 的真实应用接入、干净环境诊断和现有 Tutorial 能力矩阵验收，并发布 `v0.48.0`。

- [S-83：0.47.0 Vulkan Runtime Bundle](S-83-0.47.0-vulkan-runtime-bundle.md)——提供 Windows/Linux
  Vulkan Loader Bundle、system/bundled/auto 选择、Debug Validation Layer Bundle 和公开发布验收。

- [S-82：0.45.0 教程能力矩阵与图形特性探针](S-82-0.45.0-tutorial-capability-matrix.md)——参考 bgfx
  单项示例验证 Granit 已有能力和真实缺口，并发布 `v0.45.0`。

- [S-81：0.44.0 独立纹理变体上传](S-81-0.44.0-texture-variant-upload.md)——承接 Gneiss
  `UPSTREAM-044` 的 U44-01，支持以所选变体起点为基址上传完整变体，并发布 `v0.44.0`。

- [S-77：0.42.0 Model Viewer 架构收敛](S-77-0.42.0-model-viewer-convergence.md)——拆分共享 glTF
  渲染支持，收敛加载操作、Viewer Document、Renderer 与调度边界，删除重叠状态和转发层。
- [S-78：0.42.0 Model Viewer Linux 与 Emscripten 可靠性](S-78-0.42.0-model-viewer-platform-reliability.md)
  ——统一 ImGui 加载体验，修复 Linux 托管浏览器 WebGPU 生命周期波动，并补齐原生 Linux 验收。
- [S-79：0.42.0 ImGui Integration 教程与验收归位](S-79-0.42.0-imgui-integration-layout.md)——删除
  重叠的 ImGui 综合 Sample，以 `08_sdl_imgui` 讲解外部 SDL Window 集成，并归位自动验收。

## 最近完成

- [S-80：0.43.0 Gneiss PBR 兼容性审计](S-80-0.43.0-gneiss-pbr-compatibility.md)——修复 Vulkan
  MaterialConstants 阶段可见性和 Demote 设备特性契约，并发布 `v0.43.0`。

- [S-76：0.41.0 标准 PBR 材质正确性](S-76-0.41.0-pbr-material-correctness.md)——补齐标准
  PBR 的反射实例、Alpha、双面、逐贴图输入、顶点色与透明 HDR 阶段，并完成跨后端发布前验收。

- [S-75：0.40.0 Deferred 与多 Pass 渲染](S-75-0.40.0-deferred-rendering.md)——新增
  `07_deferred`，以 MRT G-buffer、全屏光照、调试视图和 Resize 验证现有跨后端多 Pass 能力，
  并发布 `v0.40.0`。
- [S-72：Asset Manager 与任务系统](S-72-0.39.0-asset-manager-and-task-system.md)、
  [S-73：统一交互式 Tutorial](S-73-0.39.0-unified-interactive-tutorials.md)与
  [S-74：GPU Marching Cubes](S-74-0.39.0-gpu-marching-cubes.md)——统一示例资产、任务和交互基础，
  补齐 Indirect 与 Frame Transient Buffer，并发布 `v0.39.0`。
- [S-71：0.38.0 跨后端能力契约与 WebGPU 补齐](S-71-0.38.0-webgpu-capability-parity.md)——建立
  Vulkan/WebGPU 正式能力矩阵，补齐可可靠表达的 WebGPU 能力，明确固有限制，并发布
  `v0.38.0`。
- [S-70：0.37.0 特性教程与 API 能力验证](S-70-0.37.0-feature-tutorials.md)——交付 Instancing、
  Raymarch 与 Metaballs 跨后端教程、自动验收和能力审计，并发布 `v0.37.0`。
- [S-69：0.36.0 公共 API 稳定边界与最小数学库](S-69-0.36.0-api-stability-and-math.md)——逐公共头
  确定稳定候选与实验性范围，公开轻量渲染数学函数，收敛内部、Sample 与 Tutorial 使用路径，
  并发布 `v0.36.0`。
- [S-68：0.35.0 稳定候选契约审计](S-68-0.35.0-stable-candidate-contracts.md)——建立按 component
  维护的布局与符号快照、负向行为契约、安装 SDK Consumer 和统一证据参考，并发布 `v0.35.0`。
- [S-67：0.34.0 公共 SDK 与 AssetTools 契约收敛](S-67-0.34.0-sdk-contract-convergence.md)——统一
  AssetTools 结果查询、component 级跨版本 ABI 门禁、普通 C++ 用户路径和安装 SDK 验收，并发布
  `v0.34.0`。
- [S-66：0.33.0 AssetTools 诊断与 CI 收敛](S-66-0.33.0-asset-tools-ci-convergence.md)——为
  Shader Library 源构建补齐结构化诊断，统一 CI 工具链入口、变更范围门禁和安装 SDK Shader helper，
  并发布 `v0.33.0`。
- [S-65：0.32.0 安装 SDK 图形工作流](S-65-0.32.0-sdk-graphics-workflow.md)——加固版本准备，
  补齐安装后 SDK 从 HLSL Shader Library 到首个 Draw 的独立消费与发布验收。
- [S-64：0.31.0 SDK-first 示例与资产入口](S-64-0.31.0-sdk-first-examples.md)——增加只依赖安装包
  的 Window Quickstart，统一资产职责与部署验收，并审计示例的公共 C++ 使用面。
- [S-53～S-63：0.30.0 文档、教程、Window、资产与 Model Viewer 收敛](S-53-0.30.0-documentation-tutorial-release.md)
  ——完成 Frame 生命周期文档、两个特性教程、Window Target/SDL3、统一 Example Asset System、
  唯一 Model Viewer Application 与 shared-only Release，并发布 `v0.30.0`。
- [S-52：0.29.0 版本收口与发布验收](S-52-0.29.0-release-acceptance.md)——完成迁移说明、
  跨平台矩阵、四套 SDK 候选包、校验和与正式发布复验。
- [S-51：0.29.0 Shader Library 逻辑名称](S-51-0.29.0-shader-library-logical-names.md)——把
  清单逻辑名称写入 `.grshlib`，统一运行时与 Material 工具入口，并删除独立 Shader 索引和生成 ID。
- [S-50：0.29.0 Model Viewer 教程迁移](S-50-0.29.0-model-viewer-tutorial-migration.md)——新增
  09 Model Loading，将跨后端 Model Viewer 收敛为 Tutorial 10，并删除重复 Sample。
- [S-48：线性入门教程与配套示例](S-48-0.28.0-linear-tutorial-series.md)——完成从 Window 到
  ImGui 的八章连续教程、强类型 C++ 使用面审计和桌面/浏览器自动验证。
- [S-49：0.28.0 Window 跨平台可选主循环](S-49-0.28.0-window-loop.md)——以可选托管 Loop 统一
  桌面与 Emscripten 的逐帧调度，同时保留引擎自有事件循环。
- [S-47：Window 跨平台入口收敛](S-47-window-platform-convergence.md)——让 Emscripten 与
  Win32、XCB、Wayland 共享 Window、输入和 Surface 公共流程。
- [S-45：0.26.0 文档一致性与历史收敛](S-45-0.26.0-documentation-convergence.md)——修正当前
  事实漂移，收敛文档职责与自动检查。

完整的已完成计划见[完成计划索引](completed.md)。计划文件仍保留在本目录中，便于追溯设计目标、
验收条件和最终差异；当前行为应以 Reference、Concept 和仓库实现为准。

## 已取代

- [S-46：API 教程与可运行示例阶梯](S-46-api-tutorial-example-ladder.md)——其主题式教程方案由
  S-48 的线性可见结果教程取代；已经完成的最小 Renderer 和 Triangle 实现继续复用。

## 暂缓与重新评估

- [D-09：Bindless Resource Table](D-09-bindless-resource-table.md)——等待真实绑定压力证据。
- [H-09B：透明 PBR 正确性](H-09B-transparent-pbr-correctness.md)——评估契约已经满足恢复条件，
  实现由 [S-76](S-76-0.41.0-pbr-material-correctness.md) 承接。

## 状态与维护

- **草案**：仍有影响方向的未决问题。
- **已确认**：主要设计已经同意，可以进入实施。
- **实施中**：代码或文档正在落地。
- **已完成**：验收通过并记录最终差异。
- **已暂停**：存在明确阻塞或重新评估条件。

计划完成后保留目标与最终差异，详细日志转入 Record，当前行为同步到 Reference 或 Concept。文档
结构和生命周期统一遵循[项目文档规范](../../DOCUMENTATION_GUIDE.md)。
