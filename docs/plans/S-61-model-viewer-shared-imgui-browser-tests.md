<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-61：Model Viewer 共享 ImGui 与浏览器测试边界

## 状态

**已完成，P1。** Desktop/Web 已使用同一套 ImGui Viewer 面板，浏览器测试导出与 Pipeline
C API 探针只进入测试产物。正式与测试浏览器目标以及桌面集成已在 Pull Request #85 中通过。

## 背景与目标

Desktop 已使用 `viewer_panels`、统一 Granit Input 与 Canvas 捕获，Web 仍维护一套 HTML 质量和灯光
面板。Web 正式启动还会执行包含临时 Shader、Compute Pipeline 和重复销毁检查的
`pipeline_validation`，现有 `runtime_control` 同时服务页面交互与 Playwright 验收。

本阶段完成以下收敛：

- 提取共享 `viewer_ui`，统一 ImGui Context、输入、字体、面板和 Draw Data 捕获；
- Web 删除重复 DOM 质量/灯光控件，使用与 Desktop 相同的 `viewer_panels`；
- 正式 Web 目标只运行真实 Model Viewer，测试导出和 Pipeline 探针进入独立测试目标；
- 将 `runtime_control` 改名并收窄为浏览器测试观察与控制接口。

## 非目标

- 不把 ImGui 或 Model Viewer UI 安装到 Granit SDK；
- 不让正式页面依赖 Playwright、测试计数器或临时 Shader；
- 不通过模拟 ImGui 像素坐标验证资源生命周期；
- 不修改现有 Viewer 控件语义、PBR 画面或公共 API/ABI。

## 已确认决策

`viewer_ui` 属于 Model Viewer Sample 共享层，在 UI 线程拥有 ImGui Context 与 Texture Registry，消费
Granit Window/Input 事件并输出拥有型 `frame_canvas_data`。字体像素由 UI 捕获，再经执行器交给
`render_runtime` 上传，避免 ImGui 状态跨线程借用。

正式 `granit_sample_model_viewer_web` 不导出测试函数。浏览器测试目标复用同一应用代码，通过编译
定义启用 `browser_test_api` 与 Pipeline C API 探针；Playwright 继续用函数接口控制确定性状态并检查
资源归零，不依赖 ImGui 控件坐标。

## 实施顺序

1. **S-61A 契约基线（完成）**：登记共享 UI 与测试产物边界。
2. **S-61B 共享 Viewer UI（完成）**：提取 Context、事件、字体、面板和帧捕获，迁移 Desktop。
3. **S-61C Web ImGui（完成）**：Web 接入同一 UI，删除 DOM 质量与灯光面板。
4. **S-61D 浏览器测试目标（完成）**：测试导出和 Pipeline 探针只编译到独立测试产物。
5. **S-61E 验收与文档（本地完成）**：已验证 Desktop、正式 Web、测试 Web、Chrome 和文档；
   远端 Emscripten/Linux 矩阵留待分支推送后执行。

## 测试与验收

- Desktop/Web 显示相同 Viewer 面板并使用相同输入捕获和配置结构；
- 正式 Web 产物不包含 `granit_web_*` 测试导出或临时 Pipeline 探针；
- 浏览器测试产物覆盖多帧、输入、Resize、质量、灯光、取消、错误诊断和资源归零；
- Windows Model Viewer、Emscripten 正式/测试目标、Chrome 和文档检查通过。

## 风险与未决问题

- Renderer Ready 前无法绘制 GPU ImGui，HTML 仍需保留启动和致命错误提示；
- Web 字体图集必须在 Renderer Ready 后上传，并在首次 UI 帧前注册 Texture ID；
- Viewer 面板引用 GPU Texture Preview 时必须保持 Texture Registry 与 Scene 重上传同步；
- 正式与测试目标复用同一平台实现时，应避免以宏分叉正常渲染逻辑。
