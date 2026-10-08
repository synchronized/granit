<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-92B Vulkan Debug Label 基线

## 实现

- Vulkan Command Recorder 在命令缓冲开始/结束时写入 `granit.command_recorder` label；
- Dynamic Rendering 开始/结束时写入 `granit.rendering` label；
- 现有 Vulkan 对象命名入口覆盖 Buffer、Image、Image View、Sampler、Shader、Descriptor、Pipeline、
  Command Buffer 和 Timestamp Query Pool；Pipeline/Pass 与 Command Recorder 使用稳定命名；
- 通过所属 Vulkan instance 的函数表读取 Debug Utils 命令，扩展或函数不可用时自动 no-op；
- `vulkan_device` 只保存 instance 函数表的非拥有引用，instance 生命周期仍由上层 Renderer State 管理；
- 公共 C/C++ API、安装头文件和 ABI 没有变化。

## 验证

- `clang-format --dry-run --Werror`：通过；
- `git diff --check`：通过；
- `cmake --build build/windows-clang-debug --target granit -j 4`：通过；
- `ctest --test-dir build/windows-clang-debug -R '^granit\\.renderer\\.' --output-on-failure`：3/3 通过，包含
  compute、resources 和 lifecycle diagnostic。

## 未完成项

RenderDoc 动态加载和捕获触发另见 [S-92B RenderDoc 桥接](2026-10-08-s92b-renderdoc-bridge-baseline.md)；
RenderDoc 缺失时的 no-op 仍是验收条件。
