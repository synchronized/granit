<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# D-06：基础渲染示例

## 元数据

- 设计状态：已确认
- 实现状态：已完成
- 路线图任务：D-06
- 优先级：P0
- 前置依赖：D-02、D-03、D-04、D-05、F-06、F-07
- 后续依赖：D-07

## 已实现示例

- `granit_offscreen_clear_example`：创建 Renderer、离屏 Texture 及默认 View，通过 Dynamic
  Rendering 清除颜色附件并提交。
- `granit_offscreen_triangle_example`：加载仓库预编译 SPIR-V，创建 Shader、Pipeline Layout
  与 Graphics Pipeline，使用 `gl_VertexIndex` 绘制三色三角形。
- `granit_window_clear_example`：在 Win32 窗口中完成 Surface、Swapchain、acquire、清屏、提交、
  present 和窗口尺寸变化后的重建。

所有示例只包含 `granit/granit.hpp` 和必要的标准库或平台窗口头，不包含 Vulkan 头文件，也不
访问 Granit 内部目标。着色器源码与预编译 SPIR-V 放在 `examples/assets`；普通构建不依赖
Vulkan SDK 或运行时 Shader 编译器。

## 恢复边界

窗口最小化时暂停渲染，等待恢复为非零客户区尺寸。窗口尺寸变化、SUBOPTIMAL 或 OUT_OF_DATE
会触发 Swapchain 重建。`acquired_frame` 的作用域回收保证录制途中提前失败时不会永久占用
Swapchain 图像。

## 验收

- Clang 共享库、Visual Studio 共享库和 Clang 静态库均能构建全部适用示例。
- 离屏清屏与最小三角形示例能在 Vulkan Validation Layer 下运行完成。
- Win32 窗口示例能完成真实 acquire、清屏、submit 和 present。
- 示例源文件不出现 Vulkan 类型、函数或头文件。
