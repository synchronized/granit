<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 01：创建窗口并清屏

本章从空项目开始，创建 Renderer、Window、Surface 和 Swapchain，并在帧循环中把窗口清除为固定
颜色。完成后应得到一个可以缩放、可以正常关闭的纯色窗口。

## 1. 接入 Granit

```cmake
find_package(granit CONFIG REQUIRED COMPONENTS Window)

add_executable(tutorial_01_window main.cpp)
target_compile_features(tutorial_01_window PRIVATE cxx_std_20)
target_link_libraries(tutorial_01_window PRIVATE granit::granit granit::window)
```

应用只链接 Granit 安装目标，不包含 Vulkan SDK 或平台原生窗口头。安装方法见
[构建指南](../guides/build.md)。

## 2. 创建窗口与呈现对象

```cpp
granit::window_system window_system;
check(window_system.initialize());

granit::window window;
check(window.initialize(window_system, {
    .title = "Granit Tutorial 01",
    .width = 1280,
    .height = 720,
}));

granit::renderer renderer;
check(renderer.initialize({
    .application_name = "Granit Tutorial",
    .presentation = granit::presentation_mode::enabled,
}));

granit::surface surface;
check(window.create_surface(renderer, surface));
```

Window 拥有平台窗口，Surface 只借用它。不要在 Surface 和 Swapchain 仍存活时销毁 Window。

## 3. 创建 Swapchain 和 Frame Context

```cpp
granit::swapchain swapchain;
check(swapchain.initialize(renderer, surface, {
    .width = 1280,
    .height = 720,
    .presentation = granit::present_mode::fifo,
}));

granit::frame_context frames;
check(frames.initialize(renderer));
```

每帧先处理事件并读取 framebuffer 像素尺寸。尺寸为零表示窗口最小化，此时不要获取 Swapchain 图像。

## 4. 清屏并呈现

通用 Frame 生命周期是获取、开始录制、提交和呈现；事件推进发生在 Frame 之外。第一章在“录制
命令”阶段只做 Backbuffer 清屏：

```text
每轮推进：window_system.process_events + renderer.process_events
呈现帧：  acquire → backbuffer → frame_context.begin → [录制命令] → submit → present
本章录制：begin_rendering(clear) → end_rendering
```

颜色附件使用 Backbuffer View，并设置非黑色 `clear_value`。收到 Close Requested 后退出；收到 Resize、
`out_of_date` 或 `needs_recreate` 后，等待非零尺寸并重建 Swapchain。

完整调用顺序、取消路径与恢复规则见
[Frame Context 的完整窗口帧循环](../reference/frame-context.md#完整窗口帧循环)。

## 5. 验收

- 窗口显示固定清屏颜色，不持续闪烁黑色。
- 拖动尺寸后画面覆盖整个 framebuffer。
- 最小化时不忙循环获取图像。
- 关闭窗口时没有未释放用户资源诊断。

所有权和恢复细节见 [Window](../reference/window.md)、[Swapchain](../reference/swapchain.md)和
[Frame Context](../reference/frame-context.md)。下一章将在同一帧循环中绘制三角形。

[下一章：绘制三角形](02-triangle.md)
