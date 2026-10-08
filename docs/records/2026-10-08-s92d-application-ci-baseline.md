<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-92D 应用与 CI 基线

## 已实现

- 桌面 Model Viewer 支持 `--frame-trace`、`--renderdoc=off|trigger|frame:N` 和
  `--renderdoc-path`，参数在 Renderer 创建前转换为内部环境配置；
- PBR 与 Render Pipeline 集成测试使用固定的 `test-artifacts/*-frame-trace.jsonl` 路径输出诊断附件；
- Windows/Linux CI 仅在失败重跑资源 Renderer 测试并生成小容量 Frame Trace，成功路径不生成空附件；
- RenderDoc 桥接 CTest 使用最小测试替身验证动态加载和 `TriggerCapture`，不安装或链接 RenderDoc；
- `granit_frame_trace_validator` 提供无第三方依赖的跨平台逐行 JSONL/schema 解析，Windows 测试再使用系统
  JSON 解析器验证必需字段。

## 验证

- Model Viewer 桌面参数测试通过；
- PBR Trace：182 条有效 JSONL 记录（command 49、resource 90、timestamp 43）；
- Render Pipeline Trace：75 条有效 JSONL 记录（command 9、resource 66）；
- RenderDoc substitute CTest：通过。

## 限制

浏览器 E2E 运行时需要带 GPU 的浏览器环境，本地 Emscripten 编译不能替代该验证；CI 不把浏览器调试工具
或 RenderDoc 作为构建依赖。
