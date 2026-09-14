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

## 当前验证程序

- `granit_gpu_offscreen_smoke`：创建 Renderer、离屏 Texture、Shader 和 Graphics Pipeline，清屏并
  绘制三色三角形，再回读并断言图形内外像素与行布局。
- `granit_window_renderer_test`：通过 Window component 在 Win32、XCB 与 Wayland 上完成 Surface、
  Swapchain、连续三帧 acquire、清屏、提交和 present。

这些初期示例在 0.5.0 开发阶段转为 `tests/smoke` 内部验证程序，只包含 `granit/granit.hpp`、测试
私有 Shader Asset 加载辅助代码和必要的平台窗口头，不包含 Vulkan 头文件。HLSL 作者源码与已提交
的 SPIR-V/WGSL 测试输入位于 `tests/fixtures/smoke`；普通构建不依赖运行时 Shader 编译器。正式
Pipeline 内建 Shader 和跨示例共享的 PBR 参考 Shader 分别归入
`assets/sources/shaders/pipeline` 与 `assets/sources/shaders/pbr`。

## 恢复边界

窗口最小化时暂停渲染，等待恢复为非零客户区尺寸。窗口尺寸变化、SUBOPTIMAL 或 OUT_OF_DATE
会触发 Swapchain 重建。`acquired_frame` 的作用域回收保证录制途中提前失败时不会永久占用
Swapchain 图像。

## 验收

- Clang 共享库、Visual Studio 共享库和 Clang 静态库均能构建全部适用 Smoke。
- 离屏 GPU Smoke 能在 Vulkan Validation Layer 下验证清屏、最小三角形与回读像素。
- Window component 测试能在适用平台完成真实 acquire、清屏、submit 和 present。
- 示例源文件不出现 Vulkan 类型、函数或头文件。
