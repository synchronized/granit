<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Window component

## 当前状态

Window 是独立可选 component，CMake 使用者目标为 `granit::window`。当前已实现 Win32、XCB 与
Wayland Window System、顶层窗口、轮询事件、当前状态和原生句柄查询。

Window 不依赖 Renderer 或 Vulkan。应用可以只使用 Window，也可以把查询到的原生值交给
Renderer Surface。

## 创建与销毁

C API 使用 `granit_window_system` 和 `granit_window` 两种 64 位句柄：

```c
granit_window_system_desc system_desc = GRANIT_WINDOW_SYSTEM_DESC_INIT;
granit_window_system system = GRANIT_NULL_HANDLE;
granit_window_system_create(&system_desc, &system);

granit_window_desc window_desc = GRANIT_WINDOW_DESC_INIT;
window_desc.title = "Granit";
window_desc.title_length = 6;
window_desc.width = 1280;
window_desc.height = 720;

granit_window window = GRANIT_NULL_HANDLE;
granit_window_create(system, &window_desc, &window);

granit_window_destroy(system, window);
granit_window_system_destroy(system);
```

宽高必须非零，标题是调用期间借用的 UTF-8 字节序列。当前标志支持初始可见、可调整尺寸和高 DPI；
Win32 高 DPI 窗口创建期间临时使用 Per-Monitor V2 线程上下文，不永久改变应用线程的 DPI 设置。
XCB 后端接受高 DPI 标志，但在桌面缩放协议明确前不产生 Scale 事件。

C++20 提供 move-only `granit::window_system` 和 `granit::window` RAII 包装，析构时调用对应 C API。

## 当前状态查询

`granit_window_get_state` 返回最近一次平台事件处理后的窗口状态：

- `width`、`height`：平台窗口内容坐标中的当前尺寸。
- `framebuffer_width`、`framebuffer_height`：创建或重建 Swapchain 时使用的像素尺寸。
- `content_scale_horizontal`、`content_scale_vertical`：平台内容相对基础坐标的缩放比例。

查询可以紧接窗口创建执行，不需要等待首个 Resize 或 Scale 事件。Win32 从当前窗口 DPI 初始化缩放；
XCB 和未启用缩放协议的 Wayland 返回 1.0，内容尺寸与 Framebuffer 尺寸相同。应用仍应持续处理
Resize 和 Scale 事件；查询反映最近一次已由 Granit 处理的平台状态，不主动阻塞等待新事件。

C++ 使用 `granit::window::get_state`，输出类型为 `granit::window_state`。

## 事件轮询

```c
granit_window_event event = GRANIT_WINDOW_EVENT_INIT;
while (granit_window_poll_event(system, &event) == GRANIT_SUCCESS) {
  /* 处理 event.type。 */
  event = (granit_window_event)GRANIT_WINDOW_EVENT_INIT;
}
```

队列为空返回 `GRANIT_ERROR_NOT_READY`。Win32 后端产生：

- `GRANIT_WINDOW_EVENT_CLOSE_REQUESTED`
- `GRANIT_WINDOW_EVENT_RESIZED`
- `GRANIT_WINDOW_EVENT_FOCUS_CHANGED`
- `GRANIT_WINDOW_EVENT_SCALE_CHANGED`

关闭请求不会隐式销毁窗口。Scale 事件携带相对于 96 DPI 的水平/垂直比例及新的 framebuffer
像素尺寸。原生对象变化事件已经占用稳定枚举值，但当前 Win32 后端不产生该事件。

XCB 后端产生关闭请求、尺寸变化和焦点变化事件。X11/XCB 本身没有统一可靠的每窗口缩放协议，
因此当前不伪造 Scale 事件。

Wayland 后端使用稳定版 `xdg-shell` 管理顶层窗口角色和异步 configure。创建函数在收到第一次
configure 后才返回；后续 configure 转换为尺寸和焦点事件，toplevel close 转换为关闭请求。
当前未引入 fractional-scale 协议，因此同样不伪造 Scale 事件。

## Renderer 接入

```c
void* instance = 0;
void* hwnd = 0;
granit_window_get_win32(system, window, &instance, &hwnd);
```

查询只借出原生值，Window 仍拥有 HWND。应用使用这些值调用 `granit_surface_create_win32`，并保证按
Swapchain、Surface、Window、Window System 的顺序销毁。

在 Win32 Window 上查询 XCB 或 Wayland 值返回 `GRANIT_ERROR_UNSUPPORTED`，输出参数清零。
XCB Window 可通过 `granit_window_get_xcb` 借用 connection 和 `xcb_window_t` 数值，并交给
`granit_surface_create_xcb`。未设置或无法连接 `DISPLAY` 时，创建 Window System 返回
`GRANIT_ERROR_BACKEND_UNAVAILABLE`。

Wayland Window 可通过 `granit_window_get_wayland` 借用 `wl_display*` 和 `wl_surface*`，并交给
`granit_surface_create_wayland`。Window 拥有 xdg-shell 角色及原生 Surface，调用方不得自行销毁。
自动后端在 `WAYLAND_DISPLAY` 存在时优先选择 Wayland，否则选择 XCB；应用也可在 Window System
描述中明确指定后端。

## 线程约束

Window System 记录创建线程。窗口创建、销毁、事件轮询、状态和原生值查询必须在该线程执行；跨线程
调用返回 `GRANIT_ERROR_INVALID_ARGUMENT`。销毁 Window System 会级联销毁仍存活的窗口。
