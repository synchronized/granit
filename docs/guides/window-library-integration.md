<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# SDL3 与 GLFW 窗口接入

## 适用场景

本指南说明在 `granit::integration_sdl3` 尚未实现前，如何将 SDL3 或 GLFW 创建的窗口直接连接到
Granit Surface。第三方库继续拥有窗口、事件循环与输入；Granit 只借用创建 Surface 所需的原生值。

当前直接映射关系为：

| 窗口后端 | 第三方库提供 | Granit Surface 描述 | 额外转换 |
|---|---|---|---|
| Win32 | `HINSTANCE`、`HWND` | `win32_surface_desc` | 无 |
| X11 | `Display*`、X11 `Window` | `xcb_surface_desc` | 使用 Xlib-xcb 取得 XCB connection |
| Wayland | `wl_display*`、`wl_surface*` | `wayland_surface_desc` | 无 |

这些代码是平台直连示意，不属于稳定的第三方 Integration API。正式 SDL3/ImGui 可选组件见
[S-08 计划](../plans/S-08-third-party-integrations.md)。

## 通用顺序

1. SDL3 使用 `SDL_WINDOW_VULKAN` 创建窗口；GLFW 使用 `GLFW_NO_API`，不要创建 OpenGL Context。
2. 查询窗口实际使用的 Win32、X11 或 Wayland 后端及原生值。
3. 使用对应 `surface_type` 创建 Renderer。
4. 从原生值创建 Granit Surface 和 Swapchain。
5. 在第三方库事件循环中处理 Resize、最小化和关闭。
6. 按 Swapchain、Surface、Renderer、第三方窗口的顺序销毁。

原生窗口和 display/connection 必须至少存活到 Granit Surface 销毁。不要在窗口隐藏、重建或后端
切换后继续使用之前缓存的原生值。

## SDL3

SDL3 通过 `SDL_GetWindowProperties` 返回窗口属性。该函数及下列属性自 SDL 3.2.0 起可用，并要求在
主线程调用。

### Win32

```cpp
const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
void* instance = SDL_GetPointerProperty(
    properties, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr);
void* hwnd = SDL_GetPointerProperty(
    properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);

granit::renderer renderer;
auto result = renderer.initialize({
    .application_name = "SDL3 Granit",
    .surface_types = granit::surface_type::win32,
});
granit::surface surface;
if (granit::succeeded(result))
  result = surface.initialize_win32(renderer.native_handle(), {instance, hwnd});
```

### Wayland

```cpp
const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
void* display = SDL_GetPointerProperty(
    properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
void* wl_surface = SDL_GetPointerProperty(
    properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);

granit::renderer renderer;
auto result = renderer.initialize({
    .application_name = "SDL3 Granit",
    .surface_types = granit::surface_type::wayland,
});
granit::surface surface;
if (granit::succeeded(result))
  result = surface.initialize_wayland(renderer.native_handle(), {display, wl_surface});
```

SDL3 的 Wayland `xdg_*` 对象可能在隐藏和再次显示窗口时重建。窗口重新显示后必须重新查询属性，
并在原生 `wl_surface` 已变化时重建 Granit Surface 与 Swapchain。

### X11 转 XCB

SDL3 提供 Xlib `Display*` 与 X11 `Window`，Granit XCB Surface 需要同一 display 的
`xcb_connection_t*`：

```cpp
#include <X11/Xlib-xcb.h>

const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
auto* xlib_display = static_cast<Display*>(SDL_GetPointerProperty(
    properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));
const auto x11_window = SDL_GetNumberProperty(
    properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
auto* connection = XGetXCBConnection(xlib_display);

granit::renderer renderer;
auto result = renderer.initialize({
    .application_name = "SDL3 Granit",
    .surface_types = granit::surface_type::xcb,
});
granit::surface surface;
if (granit::succeeded(result)) {
  result = surface.initialize_xcb(
      renderer.native_handle(),
      {connection, static_cast<std::uint32_t>(x11_window)});
}
```

应用需要链接 X11-xcb，但不能关闭、替换或取得 SDL3 内部 XCB connection 的所有权。

## GLFW

创建窗口前设置 `GLFW_CLIENT_API` 为 `GLFW_NO_API`。原生访问函数位于 `glfw3native.h`；包含该头前
必须定义与 GLFW 实际窗口后端一致的 `GLFW_EXPOSE_NATIVE_*` 宏。宏与 GLFW 编译后端不匹配可能
导致链接失败，因此建议按平台分别编译适配源码。

### Win32

```cpp
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>

HWND hwnd = glfwGetWin32Window(window);
HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
surface.initialize_win32(renderer.native_handle(), {instance, hwnd});
```

Renderer 创建时声明 `granit::surface_type::win32`。

### Wayland

```cpp
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3native.h>

wl_display* display = glfwGetWaylandDisplay();
wl_surface* wl_surface = glfwGetWaylandWindow(window);
surface.initialize_wayland(renderer.native_handle(), {display, wl_surface});
```

Renderer 创建时声明 `granit::surface_type::wayland`。

### X11 转 XCB

```cpp
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xlib-xcb.h>

Display* display = glfwGetX11Display();
const Window x11_window = glfwGetX11Window(window);
xcb_connection_t* connection = XGetXCBConnection(display);
surface.initialize_xcb(
    renderer.native_handle(),
    {connection, static_cast<std::uint32_t>(x11_window)});
```

Renderer 创建时声明 `granit::surface_type::xcb`，应用同时链接 GLFW 和 X11-xcb。

## Resize 与事件循环

SDL3 和 GLFW 继续负责抽取事件。使用 framebuffer 像素尺寸而不是逻辑窗口尺寸重建 Swapchain：

- SDL3：响应窗口像素尺寸变化事件，并查询当前像素尺寸。
- GLFW：使用 framebuffer size callback 或 `glfwGetFramebufferSize`。
- 尺寸为零时暂停渲染；不要创建零尺寸 Swapchain。
- `acquire` 或 `present` 返回 `out_of_date`，或者 Frame 标记需要重建时，使用最新非零尺寸重建。
- Wayland 必须遵守 compositor configure；不要把客户端请求尺寸视为已经生效的实际尺寸。

完整 Swapchain 恢复语义见 [Swapchain 参考](../reference/swapchain.md)。

## 生命周期检查

- 不缓存已经隐藏、销毁或由第三方库重建的原生窗口值。
- 不销毁或断开 SDL3/GLFW 拥有的 X11 Display、XCB connection 或 Wayland display。
- 停止对应帧循环后再销毁 Swapchain 和 Surface。
- Surface 销毁完成后才能销毁第三方窗口。
- 窗口和事件 API 的线程亲和仍由 SDL3、GLFW 与平台规则决定，Granit 内部锁不会放宽它们。

## 官方接口依据

- [SDL3 Window Properties](https://wiki.libsdl.org/SDL3/SDL_GetWindowProperties)
- [GLFW Native Access](https://www.glfw.org/docs/latest/group__native.html)
- [GLFW Window Guide](https://www.glfw.org/docs/latest/window.html)
