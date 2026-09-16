<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.24 迁移到 0.25

0.25.0 合并 Granit Window 的输入生命周期，并让普通窗口呈现入口不再要求平台原生值。
这是 0.x 源码和 C ABI 的破坏性调整。升级后重新配置 CMake、重新编译所有使用者；旧动态库
和 0.24 的对象文件不能与 0.25 混用。

## Window 与 Input

`Input` 安装 component、`granit::input` 目标、独立 `granit_input_system` 以及
`<granit/input/...>` 头已经移除。输入事件和值类型现在从 `<granit/window/input.h>` 或
`<granit/window/input.hpp>` 提供；C++ 的 `window.hpp` 会包含输入包装，C 的 `window.h`
需要使用者另行包含 `input.h`。CMake 只请求 `Window`：

```cmake
find_package(granit 0.25 CONFIG REQUIRED COMPONENTS Window)
target_link_libraries(app PRIVATE granit::window)
```

每轮先调用一次 `granit_window_system_process_events(system)`，再分别调用
`granit_window_poll_event(system, &window_event)` 和
`granit_window_poll_input_event(system, &input_event)`。两个轮询函数只读各自队列，不再隐式泵送。
键盘和指针状态分别使用 `granit_window_get_keyboard_state`、
`granit_window_get_pointer_state`，传入同一个 Window System 和目标 Window。C++ 用
`window_system::process_events()`、`poll(window_event&)`、`poll(input_event&)`、
`keyboard(...)` 和 `pointer(...)`。窗口销毁会同步清理输入状态与待处理输入事件；不再安排
Input System 的附着和销毁顺序。外部 SDL3、GLFW、Qt 窗口仍由其所有者处理输入。

## Renderer 与 Surface

`granit_renderer_desc::surface_types` 已改为同一布局位置的 `presentation_mode`；
`GRANIT_PRESENTATION_DISABLED` 是默认值，创建窗口 Surface 前必须设为
`GRANIT_PRESENTATION_ENABLED`。C++ 使用
`renderer_desc.presentation = granit::presentation_mode::enabled`。不再在 Renderer 创建时选择
Win32、XCB、Wayland 或 Canvas 来源；后端按当前构建能力启用呈现。原有字段的名称与含义均已改变，
即使结构大小相同，也须审查初始化代码并重新编译。

使用 Granit Window 时直接创建 Surface：

```c
granit_surface surface = GRANIT_NULL_HANDLE;
granit_result result = granit_window_create_surface(system, window, renderer, &surface);
```

C++ 使用 `window.create_surface(renderer.native_handle(), surface)`。Renderer 不拥有 Window；
应用仍按 Swapchain、Surface、Window、Window System 的依赖顺序销毁。

外部窗口所有者继续通过 `granit_surface_create` 与 `granit_surface_desc` 接入，但须显式包含
`<granit/renderer/native_surface.h>` 或 C++ 的 `native_surface.hpp`。原生 Window getter 须
显式包含 `<granit/window/native.h>` 或 `native.hpp`。普通 `surface.h/.hpp` 和 Window 聚合头
不再提供这些原生描述与查询。SDL3 Integration 的 `query_surface_type` 已移除，
`create_surface` 会从 SDL 窗口信息识别对应平台。

当前接口和生命周期的准确约束见 [Window](../reference/window.md)、
[Window 输入](../reference/input.md)、[Renderer](../reference/renderer.md) 与
[Surface](../reference/surface.md)。
