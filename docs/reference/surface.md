<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Surface

## 定位

Surface 表示 Renderer 与原生窗口系统之间的输出连接。公开接口只接收平台窗口句柄并返回
Granit 64 位整数句柄，不暴露 Vulkan 类型。Win32 后端已经实现；XCB 与 Wayland 已建立独立
描述结构和公共入口，后端仍在开发中，当前有效请求返回 `GRANIT_ERROR_UNSUPPORTED`。

## 启用 Win32 支持

Vulkan 平台扩展必须在创建 Instance 时启用，因此需要在 Renderer 描述中提前声明：

```c
granit_renderer_desc renderer_desc = GRANIT_RENDERER_DESC_INIT;
renderer_desc.surface_types = GRANIT_SURFACE_TYPE_WIN32_BIT;
```

未声明该位时调用 `granit_surface_create_win32` 返回 `GRANIT_ERROR_UNSUPPORTED`。声明了不受当前
平台或驱动支持的类型时，Renderer 创建返回 `GRANIT_ERROR_UNSUPPORTED`。

## C API

```c
granit_win32_surface_desc desc = GRANIT_WIN32_SURFACE_DESC_INIT;
desc.instance = hinstance;
desc.window = hwnd;

granit_surface surface = GRANIT_NULL_HANDLE;
granit_result result = granit_surface_create_win32(renderer, &desc, &surface);
if (result == GRANIT_SUCCESS) {
  granit_surface_destroy(renderer, surface);
}
```

`instance` 和 `window` 分别保存 Win32 `HINSTANCE` 与 `HWND`。Granit 只在创建调用期间借用它们，
不会取得窗口所有权。调用者必须保证窗口至少存活到 Surface 销毁。

## C++ API

```cpp
granit::surface surface;
const auto result = surface.initialize_win32(
  renderer.native_handle(),
  {.instance = hinstance, .window = hwnd});
```

`granit::surface` 是无异常、move-only RAII 类型，内部同时保存所属 Renderer 句柄，析构时调用
C API。它不拥有原生窗口。

C++ 创建 Renderer 时使用 `renderer_desc::surface_types = granit::surface_type::win32`。未来同时
启用多个窗口系统时可通过按位或组合 `surface_type`。

## Linux 公共边界

XCB 使用 `granit_xcb_surface_desc`，其中 `connection` 对应借用的 `xcb_connection_t*`，`window`
使用定宽 `uint32_t` 保存 `xcb_window_t`。Wayland 使用 `granit_wayland_surface_desc`，其中
`display` 和 `surface` 分别对应借用的 `wl_display*` 与 `wl_surface*`。公共头不包含平台头文件。

C++ 包装对应 `xcb_surface_desc`、`wayland_surface_desc`、`surface::initialize_xcb` 和
`surface::initialize_wayland`。调用方仍负责窗口创建、事件循环和原生对象生命周期。完整后端
进度见 [S-04 Linux Surface 计划](../plans/S-04-linux-surface.md)。

## 生命周期与归属

Surface 只能配合创建它的 Renderer 使用，跨 Renderer 销毁会返回
`GRANIT_ERROR_INVALID_HANDLE`。推荐在销毁 Renderer 前显式销毁全部 Surface；若仍有残留，
Renderer 会先销毁它们并使其句柄失效。不要让 Renderer 销毁与其 Surface 操作并发执行。

Surface 可以拥有多个 Swapchain；销毁 Surface 会先销毁全部所属 Swapchain 并使其句柄失效。
验证模式下，该操作会报告仍存活的 Swapchain，但不会把其借用 Backbuffer 展开成用户遗漏资源；
诊断不会阻止级联清理。
