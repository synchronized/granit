<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-92B RenderDoc 桥接

## 实现

- `src/backend/vulkan/renderdoc_bridge.{h,cpp}` 只在 Vulkan 内部运行时加载 RenderDoc；
- 不包含 `renderdoc_app.h`，不链接 RenderDoc，不向安装 SDK 暴露 RenderDoc 类型；
- 支持 `GRANIT_RENDERDOC=off`、`trigger` 和 `frame:N`；可用 `GRANIT_RENDERDOC_PATH` 指定库路径；
- 只请求 RenderDoc API 1.0.0 并调用 `TriggerCapture`，捕获由 RenderDoc 自身完成；
- DLL/SO 缺失、导出符号缺失或 API 版本不匹配时自动 no-op，并通过现有 diagnostic sink 记录；
- Vulkan Renderer 初始化和呈现路径不会因为 RenderDoc 状态失败。

## 验证

- `cmake --build build/windows-clang-debug --target granit -j 4`：通过；
- 设置 `GRANIT_RENDERDOC=trigger`，在无 RenderDoc DLL 的 Windows 环境运行 Renderer 测试：通过；
- 同时开启 Frame Trace：生成 141 条合法 JSONL 记录，并记录 RenderDoc 不可用诊断。

## 未完成项

当前桥接只实现安全的 TriggerCapture 路径，尚未提供 RenderDoc 捕获标题、Start/EndFrameCapture
窗口句柄绑定或测试替身；这些能力必须继续保持可选，不能成为构建和 CI 依赖。
