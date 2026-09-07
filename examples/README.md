<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Granit 示例

本目录只保存面向使用者的完整应用示例。单项 API、错误路径和平台能力验证应进入 `tests`，不应
为了展示数量复制成独立示例。构建与运行命令见[示例程序指南](../docs/guides/examples.md)。

## 当前示例

| 示例 | 目标 | 用途 |
|---|---|---|
| ImGui | `granit_sdl3_imgui_example`、`granit_imgui_web` | 验证 SDL3 输入、ImGui 和 Canvas 的跨后端集成 |
| Model Viewer | `granit_model_viewer_example`、`granit_model_viewer_web` | 展示模型加载、PBR、交互、异步上传和质量设置 |

## 目录职责

```text
examples/
├─ assets/       可再分发的示例输入资产
├─ framework/    多个示例复用的平台壳层和辅助代码
├─ samples/      示例内容、平台入口及自身目标声明
├─ common/       glTF 等多个示例可复用、但不属于安装 SDK 的内容
└─ platform/     保留给未来跨示例平台框架的目录
```

Model Viewer 的内容、Core、工具、验收程序和目标声明均位于 `samples/model_viewer`；旧 Core 路径
只为 Web 顶层迁移保留一层 CMake 转发，不再保存实现文件。桌面入口与 SDL3 平台壳层也已归入
`samples/model_viewer/desktop`。
浏览器资源请求、批量 Fetch、Bundle 和输入适配位于 `samples/model_viewer/web`，不再由全局
`platform` 目录持有。

`framework` 和 `samples` 都是仓库私有实现，不安装，也不构成公共 SDK。只有至少被两个真实下游
共同需要、所有权和线程语义稳定的能力，才应另行设计为 Granit 公共 API。

## 新增示例

新增示例前依次确认：

1. 内容展示的是完整集成场景，不能由现有单元测试或 Smoke 更清楚地覆盖。
2. 示例自身逻辑放入 `samples/<name>`，平台生命周期优先复用 `framework`。
3. Vulkan 与 WebGPU 入口共享状态和渲染内容，不复制两套业务实现。
4. 第三方依赖保持私有，不进入 Granit 安装导出或使用者的传递依赖。
5. 至少提供一个自动 Smoke；浏览器目标还需检查控制台错误、帧推进和显式资源释放。

当前目录迁移按 [S-36 计划](../docs/plans/S-36-0.20.0-example-framework-and-model-viewer.md)
渐进完成，不为匹配最终目录一次性重写 Model Viewer。
