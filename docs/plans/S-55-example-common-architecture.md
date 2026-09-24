<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-55：Example Common 架构整理

## 状态

**已完成。** SDL target、统一 Asset System、glTF 文档资源编排、通用 Application Host 和
Model Viewer 唯一 Application 状态机已经落地。Desktop/Web 只保留真实的平台入口与执行策略差异，
并已通过 Pull Request #85 的跨平台验收。

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
- 不把 glTF Importer、应用壳、Model Viewer 执行器或 Web 资源系统提升为公共 SDK。
- 不把 glTF 解析或 GPU 上传并入资产字节读取状态机。
- 不新增第三方依赖。
- 不改变 sample 和 tutorial 的用户可见功能；必要的内部 target 名称可以同步调整。

## 已确认决策

当前目录结构为：

```text
examples/common/
├─ application/                 # 路径不调整；内部区分 Host 与 inline 呈现
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

- `application/` 保持现有路径，继续只负责应用壳；允许配置 Renderer/Swapchain，并统一轮询异步
  资产服务，但不吸收 glTF、ImGui 或 Model Viewer 业务；
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
4. **S-55D 统一后端接口实施（已完成）**：Model Viewer 两端均使用统一 Asset System、
   `asset_batch` 和 `memory_resource_resolver`；请求提供状态、字节进度、取消、诊断和 generation
   失效保护，外部资源位置由统一函数解析。通用 Resolver 和安全资源路径归属 `assets/`，依赖方向
   已调整为 `gltf -> assets`。
5. **S-55E Model Viewer 专项分析（已完成）**：`application_core`、CPU Scene、GPU 上传与 Frame
   Packet 已经保持后端无关；重复主要位于主文档/外部资源编排、窗口呈现和输入适配。Desktop 使用
   带背压的独立渲染线程，Web 使用 inline executor 并承担 C ABI 浏览器验收，不能直接套用当前
   单线程 `application` 而丢失执行语义。S-56 已将两端输入状态机和 Window/Input 事件映射收敛为
   Viewer 私有输入累积器；进一步的运行时职责拆分转入 [S-57](S-57-model-viewer-runtime-architecture.md)。
6. **S-55F glTF 文档资源收敛（已完成）**：`document_manifest` 只扫描外部 URI，
   `document_loader` 统一主文档读取、批量加载、Resolver 提交、取消、进度和结构化失败，
   `importer` 专门将完整文档导入 CPU Scene；Desktop/Web Model Viewer 不再各自维护资源状态机。
7. **S-55G Application 能力整理（部分完成）**：已抽出不拥有 Renderer 的 `application_host`，
   统一 Window、事件循环和异步资产服务；现有 `application` 在其上提供可配置的 inline
   Renderer/Swapchain 与不依赖 Acquire 的 update hook。Web Model Viewer 已接入 Host，同时保留
   自身 inline executor 和浏览器验收接口。Desktop 已经通过 `granit::window` 的 SDL3 后端屏蔽
   平台事件和 Surface 差异，同时保留 threaded executor。当前不把 Desktop 强行接入 Host；S-57
   将在不扩大通用 Host 职责的前提下整理其独立线程壳层。
8. **S-55H 验证与文档收口（进行中）**：资产、glTF 和 Model Viewer 相关 Windows 测试、
   Emscripten 构建、浏览器 Smoke 与两个教程的浏览器测试已经通过；Linux、Windows 静态配置和
   Desktop Host 取舍留待完整任务收口。`application/`、`gltf/`、`validation/` 的路径保持不变。

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
- Desktop Model Viewer 的窗口事件循环、主线程 UI 与后台渲染线程边界不同于通用 inline
  `application`；接入 Host 前需要先证明不会引入跨线程 Renderer 调用或额外状态同步。
- 是否提升任何能力为公共 API 必须另行评估，不能由本计划推导。
