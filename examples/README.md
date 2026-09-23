<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Granit 示例

本目录保存线性教程的可运行源码和面向使用者的完整应用。渐进学习内容进入 `tutorials`，综合应用
进入 `samples`；单项 API、错误路径和平台能力验证进入 `tests`，不为展示数量复制成独立示例。
构建与运行命令见[示例程序指南](../docs/guides/examples.md)。

## 当前示例

| 示例 | 目标 | 用途 |
|---|---|---|
| ImGui | `granit_sdl3_imgui_example`、`granit_imgui_web` | 验证 SDL3 输入、ImGui 和 Canvas 的跨后端集成 |
| Model Viewer | `granit_model_viewer_example`、`granit_model_viewer_web` | 展示模型加载、PBR、交互、异步上传和质量设置 |

从窗口创建到第一个三角形等渐进内容位于 `tutorials`，其中 01 Window 和 02 Triangle 已取代旧的
Minimal Renderer 与离屏 Triangle 示例。

## 目录职责

```text
examples/
├─ assets/       可再分发的示例输入资产
├─ common/       多个示例共享、但不属于安装 SDK 的私有实现，按技术域分
│  ├─ gltf/      许可适合再分发的 glTF 加载器与资源解析
│  ├─ imgui/     ImGui 主题、Draw Data 捕获与 Texture ID 注册
│  ├─ sdl/       SDL3 窗口与 ImGui 生命周期 RAII
│  ├─ validation/ 截图与视觉回归比较
│  └─ web/       浏览器资源请求、资源包与批量 Fetch
├─ samples/      综合应用内容、平台入口及自身目标声明
│  ├─ imgui/             SDL3 + ImGui 完整集成
│  └─ model_viewer/      跨后端完整应用
└─ tutorials/    从 Window 到 ImGui 的线性教程源码
```

`common`、`samples` 与 `tutorials` 都是仓库私有实现，不安装、不导出，也不构成公共 SDK。
`common` 只保存被至少一个示例复用的能力；只有被两个真实下游共同需要、所有权和线程语义稳定的
能力，才应另行设计为 Granit 公共 API。

Model Viewer 的内容、Core、工具、验收程序和目标声明均位于 `samples/model_viewer`。
桌面入口与 SDL3 平台壳层位于 `samples/model_viewer/desktop`，浏览器入口和输入适配位于
`samples/model_viewer/web`；两个示例共用的浏览器资源支撑位于 `common/web`。

## 新增综合示例

新增示例前依次确认：

1. 内容展示的是完整集成场景，不能由现有单元测试或 Smoke 更清楚地覆盖。
2. 综合应用自身逻辑放入 `samples/<name>`，平台生命周期与横切能力优先复用 `common`。
3. Vulkan 与 WebGPU 入口共享状态和渲染内容，不复制两套业务实现。
4. 第三方依赖保持私有，不进入 Granit 安装导出或使用者的传递依赖。
5. 至少提供一个自动 Smoke；浏览器目标还需检查控制台错误、帧推进和显式资源释放。

目录分层的设计与验收边界见
[S-36 计划](../docs/plans/S-36-0.20.0-example-framework-and-model-viewer.md)。
