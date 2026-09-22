<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 路线图

本路线图只记录当前阶段、正在实施的任务、暂缓候选和长期方向，不构成版本或发布日期承诺。
已发布版本的变化见 [Changelog](../CHANGELOG.md)，详细设计见[计划索引](plans/README.md)，实际执行
证据见[实施记录](records/README.md)。

## 当前阶段

Granit 已形成桌面 Vulkan、浏览器 WebGPU、C11 ABI、C++20 RAII、Window、RenderPipeline 与
AssetTools 的完整闭环。当前仍处于 0.x，下一阶段优先提高接口、文档和发布契约的可信度，再根据
真实使用证据决定新的公共能力。

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
| SDK 与发布 | 可用 | Windows/Linux shared/static、安装审计与不可变候选晋级 |

各能力的准确使用方式和限制以 [Reference](README.md#api-与行为参考) 为准。

## 当前计划

### S-48：线性入门教程与配套示例

**状态：实施中，P1。**

[S-48](plans/S-48-0.28.0-linear-tutorial-series.md) 将现有主题式教程替换为从 Window、Triangle、
Texture、Camera、Mesh、Material/Lighting、Render Pipeline 到 ImGui 的连续路径。01～08 的正文、
配套源码与自动验证已经完成；Window 与 ImGui 章节同时覆盖浏览器 WebGPU。当前剩余 C++ 使用面
审计和发布收尾。

## 最近完成

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
| [透明 PBR](plans/H-09B-transparent-pbr-correctness.md) | 暂缓 | 产品需要正确折射、排序或大量透明 PBR 材质 |
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
- [迁移指南](guides/migrate-0.24-to-0.25.md)：最近一次破坏性版本迁移。
- [计划索引](plans/README.md)：当前与暂缓任务；[完成计划索引](plans/completed.md)保存已验收计划。
- [实施记录](records/README.md)：跨平台、性能和发布验收证据。

扩大公共 API 前应先更新对应 Plan 和本路线图状态，并明确验证与退出条件。
