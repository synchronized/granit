<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-37H：WebGPU Provider 边界收敛

## 状态

**实现中。** 设计已确认并纳入 0.21.0；S-37H1 已完成，当前迁移资源与 Shader。

## 背景与目标

WebGPU 已由 [ADR-005](../decisions/ADR-005-browser-only-webgpu.md) 收敛为 Emscripten 浏览器后端，
随 Granit 静态编译并直接链接 Emdawnwebgpu。现有 `provider_api.h`、Provider ABI 版本、函数表查询和
`provider_dispatch` 来自早期可插拔桌面 Provider 设计，在当前部署模型中不再隔离动态模块，反而让
每次内部调用经过整数句柄、C 函数表和重复能力校验。

本任务删除这层历史边界，将真实 WebGPU 实现改为后端私有的 C++ Context。HAL 契约继续隔离
Renderer 与具体后端，资源、命令、Pipeline、呈现、Shader 和 Timestamp adapter 继续划分实现职责。
目标调用路径为：

```text
Renderer Registry -> 私有 HAL -> WebGPU renderer state / domain adapter
                  -> WebGPU context -> Emdawnwebgpu
```

## 非目标

- 不修改公共 C API、C++ 包装或动态库 ABI。
- 不改变 Vulkan 后端行为，也不把 WebGPU 类型放入公共头文件或通用 HAL 契约。
- 不恢复桌面 Dawn、动态 Provider、wgpu-native、Android 或新的 WebGPU 部署形式。
- 不增加新的渲染能力，不借此改写资源状态、同步或 Pipeline 语义。
- 不把已经拆分的 domain adapter 合并回 `webgpu_renderer_state`。

## 已确认决策

- 保留 `backend/contracts`、Renderer Registry、`webgpu_renderer_state` 和按领域拆分的 adapter。
- 删除内部版本化 Provider ABI、查询符号、函数表与 `provider_dispatch`；私有实现无需兼容旧入口。
- 将 `provider.cpp` 中的真实 Emdawnwebgpu 逻辑迁移为后端私有 C++ Context，不只进行文件改名。
- Context 直接拥有 Instance、Adapter、Device、Queue、Surface 和异步回调状态；adapter 通过明确的
  C++ 方法调用它。
- Provider 整数句柄逐领域替换为后端私有强类型对象，保持 generation、所有权和延迟销毁校验。
- 保留当前异步初始化、Device Lost、诊断、浏览器 Surface 和 Pipeline 异步创建语义。
- 每个迁移阶段保持可构建，并运行对应单元测试；涉及呈现和异步路径后运行浏览器 WebGPU Smoke。

## 实施顺序

1. **S-37H1 Context 与生命周期（已完成）**：Renderer factory 已直接创建静态 WebGPU 后端，实例
   生命周期由内部入口接入，不再查询导出符号或按 Provider ABI 版本选择实现。
2. **S-37H2 资源与 Shader**：让 resource、shader 和 timestamp adapter 直接调用 Context，使用
   后端私有类型表达 Buffer、Texture、View、Sampler、Shader 与 Query 资源。
3. **S-37H3 Pipeline 与命令**：迁移 Bind Group、Pipeline、Command Encoder、Render/Compute Pass、
   提交、复制和回读路径，并保持异步 Pipeline 的资源保活与完成通知。
4. **S-37H4 呈现与帧生命周期**：迁移 Surface 配置、Backbuffer 获取、Present、帧完成和 Device Lost
   路径，完成浏览器窗口与离屏渲染回归。
5. **S-37H5 删除历史边界**：删除 `provider_api.h`、`provider_dispatch.*`、Provider ABI 版本、函数表、
   查询符号和只验证该边界的测试；清理 CMake 与命名。
6. **S-37H6 文档与发布验收**：更新架构概念与实现状态，完成 Emscripten、浏览器 WebGPU、Vulkan、
   Windows 共享/静态及 Documentation 回归，并并入 S-37G 发布验收。

各阶段允许根据真实依赖调整 adapter 的迁移分组，但不得长期保留 Context 与 Provider 两套平行调用
路径。过渡代码只在同一特性分支内存在，不形成兼容承诺。

## 测试与验收

- Emscripten 配置与完整构建通过，WebGPU 后端不再编译或引用 Provider ABI 和 dispatch。
- 浏览器 WebGPU 平台 Smoke、公共 Renderer 测试、Render Pipeline Consumer 和固定画面像素回归通过。
- Shader Library 在 WebGPU 上继续自动选择 WGSL；缺少兼容载荷仍返回既有错误码。
- Buffer、Texture、Pipeline、Command、Swapchain、Timestamp 和异步操作覆盖成功、失败及销毁路径。
- Device Lost 与未捕获错误继续进入统一诊断，异步回调不会访问已销毁的 Renderer 或资源。
- Vulkan 与 Windows 共享/静态测试保持通过，公共 ABI 快照与导出符号不变化。
- `rg` 检查确认源码和构建文件中不存在 `provider_api`、`provider_dispatch`、Provider ABI 版本或查询
  符号的残留引用。

## 风险与未决问题

- `provider.cpp` 覆盖资源、命令、Pipeline、呈现和异步回调，直接一次性替换容易破坏生命周期；按
  领域迁移并在每阶段保持测试可运行。
- Emdawnwebgpu 对象的引用计数和回调 user data 生命周期必须由 Context 明确拥有，不能因去掉整数
  句柄表而放松悬空访问检查。
- 浏览器 WebGPU 是此实现的唯一运行平台，关键行为不能只依赖主机侧 mock；呈现、异步与 Device Lost
  相关阶段必须运行真实浏览器回归。
- 若迁移发现某项函数表确实隔离了独立生命周期或并发约束，应把该约束转为 Context 的显式 C++ 接口，
  不恢复版本化 Provider ABI。
