<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-17：Render Pipeline 内部职责收敛

## 状态

- 实现状态：实现中
- 前置依赖：H-07、S-13、S-15
- 优先级：P1

## 背景与目标

公共 Render Pipeline 已形成可运行的 Forward PBR 路径，但内部 API 文件仍同时承担句柄注册、
View 数据准备、光照转换、具体 Draw 录制、Tone Mapping 和 GPU 指标管理。目标是在不改变公共
API、ABI 与渲染结果的前提下，让入口只负责校验、生命周期和整帧编排。

## 非目标

- 不改变 Scene、Mesh、Material 或 Render Pipeline 公共数据模型。
- 不把 Gneiss 等上游项目的 ECS、资产或场景所有权移入 Granit。
- 不改变 Shader Binding、Render Graph 拓扑或提交粒度。
- 不在本任务拆分 Vulkan 与 WebGPU 后端大文件。

## 已确认决策

- `render_view_submission` 负责 View 可见对象、payload 和 Draw Binding 的短期提交表示。
- `lighting_submission` 负责光源筛选、GPU 布局打包与 IBL 参数准备。
- Material 继续通过现有 `material_access` 解析，不建立重叠的 Resolver。
- Forward、Shadow 与 Tone Mapping 录制分别进入私有组件，共享状态集中定义但不公开。
- GPU Timestamp 槽管理进入 Metrics 私有组件；公共查询 API 保持不变。
- `src/renderer` 只能依赖私有 HAL 契约，不包含具体 WebGPU Provider 或 Vulkan 类型。

## 实施顺序

1. **S-17A：提交数据分层**——提取 View 与 Lighting Submission。（已完成）
2. **S-17B：Registry 边界修正**——移除通用 Registry 对 WebGPU Provider 头的依赖。（已完成）
3. **S-17C：Draw Recorder 拆分**——分别提取 Forward PBR 与 Shadow Draw 录制。（已完成）
4. **S-17D：后处理与指标拆分**——提取 Tone Mapping 录制与 Timestamp 槽管理。（已完成）
5. **S-17E：验收**——确认入口仅保留参数校验、句柄生命周期、Render Graph 和回调编排。

## 测试与验收

- Windows Clang/MSVC 的 Render Pipeline、Lighting 和相关离屏示例通过。
- Emscripten WebGPU Release 构建及浏览器模型查看器冒烟测试通过。
- `render_pipeline_api.cpp` 不再实现具体 Forward、Shadow、Tone Mapping Draw 过程。
- `src/renderer` 不直接包含具体后端 Provider/Vulkan 头。
- 公共头文件、ABI 快照和安装导出不发生变化。

## 风险与未决问题

- Recorder 会共享 Renderer、缓存和 Uniform Arena 生命周期，拆分时必须避免复制运行时状态。
- 本轮只移动既有行为；每 View 共享 Lighting Bind Group 等资源优化需要独立测量后实施。
- Vulkan/WebGPU 大文件按能力域对称拆分，待本任务完成后另行排期。
