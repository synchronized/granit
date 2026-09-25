<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Granit 示例

本目录保存特性教程的可运行源码和面向使用者的完整应用。聚焦单条渲染路径的内容进入
`tutorials`，完整应用与集成示例进入 `samples`；单项 API、错误路径和平台能力验证进入 `tests`，不为
展示数量复制成独立示例。
构建与运行命令见[示例程序指南](../docs/guides/examples.md)。

只使用安装后 SDK 的外部工程位于 [`standalone/window_clear`](standalone/window_clear) 与
[`standalone/triangle`](standalone/triangle)。它们不参与 Granit 主工程构建，用于验证普通 Consumer
能独立完成窗口清屏，以及从 HLSL Shader Library 到 Draw 和像素回读。

## 当前综合入口

| 入口 | 位置 | 目标 | 用途 |
|---|---|---|---|
| ImGui | `samples/imgui` | `granit_sdl3_imgui_example`、`granit_imgui_web` | 验证 SDL3、ImGui 和 Canvas 集成 |
| Model Viewer | `samples/model_viewer` | `granit_sample_model_viewer`、Web 目标 | 展示完整跨后端 PBR 工具 |

低层纹理立方体、PBR 资产、Instancing、Raymarch 和 Metaballs 位于 `tutorials`，公共应用生命周期
位于 `common/application`。

## 目录职责

```text
examples/
├─ assets/       按 tutorials/samples 组织的可再分发输入资产
├─ standalone/   只依赖安装 SDK 的独立 Consumer 工程
├─ common/       多个示例共享、但不属于安装 SDK 的私有实现，按技术域分
│  ├─ gltf/      glTF/GLB 文档资源编排、格式导入、图片解码与 CPU Scene 转换
│  ├─ assets/    统一的 Desktop/Web 异步读取、资源批次、内存 Resolver 与运行时资产
│  ├─ application/ 跨平台应用 Host、inline 呈现与异步资产服务生命周期
│  ├─ imgui/     ImGui 输入、字体图集、Draw Data 捕获与 Texture ID 注册
│  ├─ model_viewer/ 教程与完整查看器共用的 glTF → GPU Scene 映射
│  ├─ sdl/       SDL3 窗口与 ImGui 生命周期 RAII
│  └─ validation/ 截图与视觉回归比较
├─ samples/      综合应用内容、平台入口及自身目标声明
│  ├─ imgui/      SDL3 + ImGui 完整集成
│  └─ model_viewer/ 完整跨后端模型查看器
└─ tutorials/    编号特性教程源码
```

`common`、`samples` 与 `tutorials` 都是仓库私有实现，不安装、不导出，也不构成公共 SDK。
`common` 只保存被至少一个示例复用的能力；只有被两个真实下游共同需要、所有权和线程语义稳定的
能力，才应另行设计为 Granit 公共 API。

示例资产统一使用 Mount 与逻辑路径，Desktop/Web 的文件读取、Fetch 和部署差异由私有实现处理；
具体边界见 [Example Asset System 契约](common/assets/README.md)。

Model Viewer 的内容、Core、工具和验收程序均位于 `samples/model_viewer`。桌面入口与 SDL3
平台壳层位于其 `desktop` 子目录，浏览器入口和输入适配位于 `web`；
跨教程复用的 GPU Scene 位于 `common/model_viewer`，跨平台资源读取位于 `common/assets`。

## 新增综合示例

新增示例前依次确认：

1. 内容展示的是完整集成场景，不能由现有单元测试或 Smoke 更清楚地覆盖。
2. 综合应用自身逻辑放入 `samples/<name>`，平台生命周期与横切能力优先复用 `common`。
3. Vulkan 与 WebGPU 入口共享状态和渲染内容，不复制两套业务实现。
4. 第三方依赖保持私有，不进入 Granit 安装导出或使用者的传递依赖。
5. 至少提供一个自动 Smoke；浏览器目标还需检查控制台错误、帧推进和显式资源释放。

目录分层的设计与验收边界见
[S-36 计划](../docs/plans/S-36-0.20.0-example-framework-and-model-viewer.md)。
