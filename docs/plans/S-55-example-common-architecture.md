<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-55：Example Common 架构整理

## 状态

**实施中。** SDL target 与 Desktop/Web 统一资产加载接口已经完成，原 `common/web` 的通用请求、
批次和内存 Resolver 已收敛到 `assets/`。下一阶段专项分析 `model_viewer/`，再决定是否重构。

## 背景与目标

`examples/common` 已经承载应用生命周期、资产加载、平台桥接、ImGui、Model Viewer 渲染和
视觉验证等多类代码。当前优先问题是 `assets/` 与 `web/` 的边界不够清晰，sample/tutorial 可能
需要直接知道后端；`sdl/` 只有游离头文件，没有明确的 CMake target。`application/`、`gltf/` 和
`validation/` 当前职责已经相对稳定，`model_viewer/` 则需要在了解实际依赖后单独设计。

本计划的目标是建立一套可以长期扩展的示例私有架构：

- 保留现有短目录路径，避免为了形式上的分层增加无意义的路径深度；
- 让资产和 Web 资源提供与后端无关的接口，sample/tutorial 不直接处理后端差异；
- 为 SDL 生命周期建立清晰的 CMake target 和依赖边界；
- 通过实际消费者和依赖图判断是否需要合并 `assets/` 与 `web/` 内容；
- 在充分分析前不改动 `model_viewer/` 的目录和实现结构；
- `examples/common` 仍保持私有，不安装、不导出、不形成公共运行时。

## 非目标

- 不修改 Granit 公共 C/C++ API、ABI 或 Vulkan 封装边界。
- 不把 glTF Loader、应用壳、Model Viewer 执行器或 Web 资源系统提升为公共 SDK。
- 不把 glTF 解析或 GPU 上传并入资产字节读取状态机。
- 不新增第三方依赖。
- 不改变 sample 和 tutorial 的用户可见功能；必要的内部 target 名称可以同步调整。

## 已确认决策

当前目录结构为：

```text
examples/common/
├─ application/                 # 暂不调整
├─ assets/                      # Desktop/Web 统一资产加载、批次和 Resolver
├─ gltf/                        # glTF/GLB 格式导入层，路径不调整
├─ imgui/                       # 暂不调整
├─ model_viewer/                # 后续重点分析，暂不预设拆分方案
├─ sdl/                         # 增加明确的 CMake target
└─ validation/                  # 暂不调整
```

稳定的依赖方向调整为：

```text
assets ─→ gltf ──────────────┐
application / imgui / sdl ───┼─→ samples / tutorials
validation ──────────────────┘
```

具体约束如下：

- `application/` 暂不改变现有路径和职责；
- `gltf/` 保持现有路径和导入职责，资源读取改为依赖 `assets/` 的通用抽象；
- `validation/` 暂不改变现有路径和实现；
- `sdl/` 形成明确 target，但不引入 sample 业务逻辑；
- `assets/` 通过同一接口选择 Desktop 文件读取或 Emscripten Fetch 后端；
- 通用资源路径与 `resource_resolver` 抽象归 `assets/`，`gltf/` 作为格式导入层依赖它；
- sample/tutorial 依赖统一的资源加载接口，不直接分支处理文件读取或 Emscripten Fetch；
- glTF 解析进度和 GPU 上传进度保持独立，由上层按阶段组合展示；
- `model_viewer/` 暂不拆分，先记录模块职责、依赖、所有权和线程边界。

## 实施顺序

1. **S-55A SDL target 整理（已完成）**：`sdl/` 已增加 `granit_example_sdl`，统一传播 SDL3、
   ImGui SDL3 后端和公共头路径；ImGui 与 Model Viewer 桌面 sample 均通过该 target 使用生命周期
   类型，Model Viewer 不再直接编译后端源码。
2. **S-55B 现状与依赖盘点（已完成）**：确认请求状态、批次和内存 Resolver 与平台无关，只有
   Emscripten Fetch 属于 Web 后端；HTML Shell 属于具体 sample。
3. **S-55C 资产/Web 边界设计（已完成）**：通用能力归入 `assets/`，构建时选择 Desktop 文件
   后端或 Web Fetch 后端；原 `common/web` 不再保留平行状态系统。
4. **S-55D 统一后端接口实施（已完成）**：Model Viewer 两端均使用 `asset_loader`、
   `asset_batch` 和 `memory_resource_resolver`；请求提供状态、字节进度、取消、诊断和 generation
   失效保护，外部资源位置由统一函数解析。通用 Resolver 和安全资源路径归属 `assets/`，依赖方向
   已调整为 `gltf -> assets`。
5. **S-55E Model Viewer 专项分析**：单独记录 `gpu_scene` 的数据流、资源所有权、上传阶段、
   线程边界和测试缺口，再提出是否拆分及拆分方式。
6. **S-55F 验证与文档收口（进行中）**：接口测试、Windows Clang 构建、Emscripten 构建、
   浏览器 Smoke 和示例文档已经完成；Linux 与 Windows 静态配置留待完整任务收口时验证。
   `application/`、`gltf/`、`validation/` 的路径保持不变，除非专项分析证明必要。

## 测试与验收

- `sdl/` 有明确 target，SDL/ImGui 依赖不会通过无关 target 传播；
- `assets/` 统一拥有平台无关状态和平台后端，sample/tutorial 不需要直接判断资源后端；
- 文件系统、Web 资源请求、内存 resolver 和失败路径可以独立测试；
- 无 native window 环境可以运行适用的纯逻辑测试；
- `application/`、`gltf/`、`validation/` 保持用户可见行为和目录路径不变；
- Model Viewer 在专项分析完成前不发生未经设计的结构拆分；
- Windows 动态库、Windows 静态库、Linux 和 Emscripten 目标构建通过；
- sample 和 tutorial 行为保持不变，示例私有 target 不进入安装导出；
- `git diff --check`、文档链接检查和相关 CTest 通过。

## 风险与未决问题

- Desktop 后端以后台文件读取发布进度，Web 后端依赖浏览器是否提供响应总长度；未知总长度不得
  伪造百分比。
- 逻辑取消通过 generation 保证迟到结果失效；Web Fetch 当前不承诺立即终止底层网络传输。
- Model Viewer 的资源所有权、线程边界和上传阶段尚未完成专项分析，暂不提前决定文件拆分。
- 是否提升任何能力为公共 API 必须另行评估，不能由本计划推导。
