<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# ADR-001：调试绘制与文字渲染模块边界

- 状态：已接受
- 日期：2026-08-14

## 背景

H-06 已建立 Unlit、UI Draw List、动态几何上传和 Tone Mapping 后扩展点。线框、Gizmo、文字与
ImGui 都能复用这些能力，但它们拥有不同的数据来源和生命周期。若全部并入 Renderer，会让底层
GPU API 承担字体解析、文字布局和编辑器状态；若各自建立渲染路径，则会复制资源与批处理系统。

## 决策

采用“上层生成几何，H-06 负责绘制”的边界：

```text
调试命令 ─> Debug Draw 几何生成器 ─┐
文字/字体 ─> 整形、栅格化、Atlas ──┼─> UI Draw List / Unlit Pass ─> UI 扩展点
第三方 UI ─> 专用适配层 ───────────┘
```

- Renderer 只管理 GPU 资源、命令和同步，不保存调试图元、字体或 UI 状态。
- Debug Draw 是可选上层模块。世界空间线段、三角形和 Gizmo 生成临时 Mesh/Unlit Draw；屏幕空间
  图元生成 UI Draw List。命令默认只在当前帧有效，持久化由调用方明确管理。
- Text 是独立可选模块。它负责字体数据、文字整形、字形栅格化、Atlas 缓存与失效，并把已定位
  字形输出为 UI 四边形；Renderer 和 Render Pipeline 不接收 UTF-8 字符串或字体对象。
- 首版文字接口不自行实现 Unicode 整形算法。实现阶段再评估 FreeType 类栅格化库与 HarfBuzz 类
  整形库，并单独记录版本、许可证、构建和跨平台影响；本决策不提前增加依赖。
- ImGui、SDL 等第三方系统通过独立适配目标转换已有 Draw Data 或窗口句柄，不成为 Granit 核心的
  传递依赖。
- 字形 Atlas 使用普通 Granit Texture、Texture View 和 Sampler，按 Renderer 所有；缓存策略属于
  Text 模块，不进入 C ABI 句柄表的基础资源语义。

## 影响

- Unlit/UI Shader、批处理、Scissor 和显示空间颜色规则可被调试绘制、文字与第三方 UI 共用。
- 核心动态库保持精简，使用者可只安装 Renderer 或 Render Pipeline component。
- 公共 UI Draw List、Debug Draw 和 Text C ABI 必须分别设计；内部原型不能直接视为稳定公共接口。
- 文字质量和国际化能力取决于可选 Text component，而不是 Renderer 的设备能力。

## 替代方案

- **全部并入 Renderer**：调用入口集中，但污染底层职责并强制引入字体和 UI 依赖，因此拒绝。
- **每个功能独立录制 Vulkan**：短期直接，但绕过 Granit 封装并复制同步和资源管理，因此拒绝。
- **内建完整 UI 框架**：超出渲染库定位；布局、输入与控件树继续由上层或第三方库负责。
