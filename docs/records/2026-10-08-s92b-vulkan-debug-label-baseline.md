<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-92B Vulkan Debug Label 基线

## 实现

- Vulkan Command Recorder 在命令缓冲开始/结束时写入 `granit.command_recorder` label；
- Dynamic Rendering 开始/结束时写入 `granit.rendering` label；
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

本记录不代表 RenderDoc 桥接已经完成。动态加载、捕获触发和对象/Pass/Pipeline 命名统一仍在 S-92B
后续阶段实现；RenderDoc 缺失时的 no-op 要继续保持为验收条件。
