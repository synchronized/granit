<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 公共 API 稳定等级

本文是安装 SDK 公共头稳定等级的权威清单。Granit 当前仍处于 0.x，以下等级表达长期保留意图和
变更门槛，不构成已经冻结的 ABI；正式兼容承诺仍以未来发布说明为准。

## 等级定义

| 等级 | 含义 | 0.x 变更要求 |
|---|---|---|
| 长期候选 | 已具备稳定范围评审条件，设计目标是进入首个稳定版本 | 优先兼容扩展；破坏性变化必须有明确决策、Changelog、迁移指南和同步契约基线 |
| 观察候选 | 已有自动化契约，但仍需真实 Consumer 验证边界 | 可以有记录地调整，不得静默改变所有权、线程、错误或资产语义 |
| 实验性 | 仍允许根据工具链或第三方依赖演进 | 可以在 0.x minor 中破坏，必须记录影响和迁移方式 |
| 内部 | 不属于安装 SDK 公共接口 | 无兼容承诺 |

计划移除长期候选接口时，应至少提前一个 minor 在头文件与 Changelog 标记弃用；已发布的 C 导出
在正式稳定前仍受跨版本符号门禁保护。缺陷修复和安全恢复不要求维持错误行为，但必须增加行为测试。

## 公共头分类

| 路径或入口 | 等级 | 范围 |
|---|---|---|
| `granit/granit.h`、`granit/granit.hpp` | 长期候选 | Core 聚合入口 |
| `granit/core/*` | 长期候选 | 版本、结果、诊断、基础类型、内容 ID 与 Shader 公共值类型 |
| `granit/math/*` | 长期候选 | C ABI 数学值类型和 C++20 最小渲染数学函数 |
| `granit/renderer/*` | 长期候选 | Renderer、资源、Shader、Pipeline、命令、帧、Surface 与 Swapchain |
| `granit/window.h`、`granit/window.hpp`、`granit/window/*` | 长期候选 | Window System、事件、输入、Loop、呈现与原生值查询 |
| `granit/pipeline/*` | 观察候选 | Material、Scene、RenderPipeline、Canvas、Debug 与 Text |
| `granit/asset_tools/*` | 实验性 | 离线 Shader、Material、Texture 与 Environment 构建和检查 |
| `granit/integrations/*` | 实验性 | SDL3、ImGui 等第三方适配层 |
| `src/`、`examples/`、`tests/` | 内部 | 后端、私有模块、示例框架与测试支持代码 |

`renderer/native_surface.h` 和 `window/native.h` 属于长期候选中的显式平台入口。长期保留的是统一的
版本化描述、查询函数、借用期和错误语义；其中的原生对象仍由宿主平台管理，不能跨平台解释。
它们不等同于 Vulkan/WebGPU 后端互操作，公共接口仍不暴露 `Vk*`、WebGPU 原生对象或后端资源。

## Window 平台支持

| Backend | 平台 | 当前支持等级 | 宿主边界 |
|---|---|---|---|
| Win32 | Windows | 官方长期候选 | Window System 创建线程拥有窗口、事件泵与原生值查询 |
| XCB | Linux/X11 | 官方长期候选 | 依赖有效 `DISPLAY`；没有可靠协议时不伪造缩放事件 |
| Wayland | Linux | 官方长期候选 | 依赖 `xdg-shell` configure；当前不承诺 fractional-scale |
| Emscripten | 浏览器 | 官方长期候选 | Canvas 与 DOM 生命周期由页面宿主拥有；托管 Loop 异步返回 |
| SDL3 | 桌面与浏览器 | 可选实现，观察中 | 枚举和未启用时的 `UNSUPPORTED` 语义保留，运行时受 SDL3 部署约束 |

所有 Window 对象操作、事件处理、状态查询和原生值查询都在 Window System 创建线程执行。Window
创建的 Surface 由 Renderer 拥有；宿主按 Swapchain、Surface、Window、Window System 顺序释放。
更完整行为见 [Window component](window.md)和[线程安全约定](thread-safety.md)。

## Component 关系

- Core 与 Window 已进入长期范围评审；新增接口需要说明是否属于长期候选。
- RenderPipeline 继续观察 Material、Scene 和同步 Render 门面，不能从 Core 状态推导其已稳定。
- AssetTools 与 Integration 不阻塞 Core/Window 的稳定决策，也不会因统一 Granit 版本自动晋级。
- 新增安装公共头若未归入上述路径和等级，自动契约检查会失败。

兼容版本规则、结构扩展方式和首个稳定版本门槛见[版本与兼容策略](compatibility.md)，当前自动化
证据见[稳定候选契约证据](stable-candidate-evidence.md)。
