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

granit::window_state window_state;
check(window.get_state(window_state));

granit::renderer renderer;
check(renderer.initialize({
    .application_name = "Granit Tutorial",
    .presentation = granit::presentation_mode::enabled,
}));

granit::surface surface;
check(window.create_surface(renderer, surface));
```

Window 拥有平台窗口，Surface 只借用它。不要在 Surface 和 Swapchain 仍存活时销毁 Window。
`window_desc` 的宽高是窗口内容尺寸；创建和重建 Swapchain 时应使用 `window_state` 中的
`framebuffer_width` 和 `framebuffer_height`。两者在高 DPI 环境下可能不同。

## 3. 创建 Swapchain 和 Frame Context

```cpp
granit::swapchain swapchain;
check(swapchain.initialize(renderer, surface, {
    .width = window_state.framebuffer_width,
    .height = window_state.framebuffer_height,
    .presentation = granit::present_mode::fifo,
}));

granit::swapchain_info swapchain_info;
check(swapchain.query_info(swapchain_info));

granit::frame_context frames;
check(frames.initialize(renderer));
```

每帧先处理事件并读取 framebuffer 像素尺寸。尺寸为零表示窗口最小化，此时暂停获取 Swapchain
图像。驱动可能调整实际交换链范围，因此录制命令使用 `swapchain_info` 的宽高。

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

事件队列为空会返回 `not_ready`，这是本轮事件已经读取完毕，并不是运行失败。其他轮询错误仍应停止
程序。按下 Escape 也会退出示例。

获取成功的 Frame 必须提交并呈现，或者在录制失败时中止 Recording 并取消 Frame。C++ 包装会在
析构时兜底清理，本章源码仍显式执行失败清理，使帧生命周期可以从代码中直接看出。

完整调用顺序、取消路径与恢复规则见
[Frame Context 的完整窗口帧循环](../reference/frame-context.md#完整窗口帧循环)。

## 5. 构建和运行

仓库内的[完整源码](../../examples/tutorials/01_window/main.cpp)与本章保持同步。Windows Clang：

```powershell
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug --target granit_tutorial_01_window
.\build\windows-clang-debug\bin\granit_tutorial_01_window.exe
```

Linux Clang：

```sh
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug --target granit_tutorial_01_window
./build/linux-clang-debug/bin/granit_tutorial_01_window
```

请从 preset 对应的 `bin` 目录运行生成程序。共享库构建会把 Granit 动态库输出到同一目录，直接从
其他目录复制可执行文件可能导致系统找不到 DLL 或共享库。

仓库测试使用 `--smoke-test` 渲染三帧并完成一次 Swapchain 重建后自动退出。该参数用于自动验收，
正常阅读和运行教程时不需要传入。

## 6. 验收与排错

- 窗口显示固定清屏颜色，不持续闪烁黑色。
- 拖动尺寸后画面覆盖整个 framebuffer。
- 最小化时不忙循环获取图像。
- 点击关闭按钮或按下 Escape 后正常退出。
- 关闭窗口时没有未释放用户资源诊断。

若 Renderer 初始化报告当前环境不可用，应先确认 Vulkan 1.3 驱动和设备满足要求。窗口缩放后退出
通常表示应用没有处理 `out_of_date`；窗口只清除一部分通常表示渲染区域仍使用初始窗口尺寸，而非
重建后查询得到的 Swapchain 尺寸。

所有权和恢复细节见 [Window](../reference/window.md)、[Swapchain](../reference/swapchain.md)和
[Frame Context](../reference/frame-context.md)。下一章将在同一帧循环中绘制三角形。

[下一章：绘制三角形](02-triangle.md)
