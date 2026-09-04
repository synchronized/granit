<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Surface

## 定位

Surface 表示 Renderer 与原生窗口系统之间的输出连接。公开接口只接收平台窗口句柄并返回
Granit 64 位整数句柄，不暴露 Vulkan 或 WebGPU 类型。桌面 Vulkan 与浏览器 WebGPU 均通过相同的
Win32、Linux XCB 和 Wayland 公共入口创建 Surface；实际可用性取决于所选后端、平台开发包、
运行时扩展及 Provider 能力。

Canvas Surface 是 WebGPU 浏览器后端的公共输出入口。它与原生窗口 Surface 共用同一类句柄，
但 Vulkan Renderer 不支持创建 Canvas Surface，并返回 `GRANIT_ERROR_UNSUPPORTED`。

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

C++ 创建 Renderer 时使用 `renderer_desc::surface_types = granit::surface_type::win32`。同时启用
多个窗口系统时可通过按位或组合 `surface_type`。

## Linux 公共边界

XCB 使用 `granit_xcb_surface_desc`，其中 `connection` 对应借用的 `xcb_connection_t*`，`window`
使用定宽 `uint32_t` 保存 `xcb_window_t`。Wayland 使用 `granit_wayland_surface_desc`，其中
`display` 和 `surface` 分别对应借用的 `wl_display*` 与 `wl_surface*`。公共头不包含平台头文件。

C++ 包装对应 `xcb_surface_desc`、`wayland_surface_desc`、`surface::initialize_xcb` 和
`surface::initialize_wayland`。调用方仍负责窗口创建、事件循环和原生对象生命周期。完整后端
进度见 [S-04 Linux Surface 计划](../plans/S-04-linux-surface.md)。

Linux 构建默认探测 XCB 和 Wayland Client 开发包；找到时启用对应后端，找不到时保留公共入口并
返回不支持。可分别通过 `GRANIT_ENABLE_XCB=OFF`、`GRANIT_ENABLE_WAYLAND=OFF` 显式关闭。
平台头文件、链接库与 Vulkan 平台定义不会传播给使用者。

## Canvas 公共边界

创建 Renderer 时可通过 `GRANIT_SURFACE_TYPE_CANVAS_BIT` 声明 Canvas 输出需求。Canvas 创建
描述使用 CSS selector 定位页面元素：

```c
granit_canvas_surface_desc desc = GRANIT_CANVAS_SURFACE_DESC_INIT;
desc.selector = "#viewport";
desc.selector_length = 9;

granit_surface surface = GRANIT_NULL_HANDLE;
granit_result result = granit_surface_create_canvas(renderer, &desc, &surface);
```

`selector` 使用“指针 + UTF-8 字节长度”，不要求以空字符结尾；Granit 只在创建调用期间读取内容，
不会保留调用方指针。`selector == NULL` 且 `selector_length == 0` 时使用默认值 `#canvas`。空选择器、
超过 4096 字节或包含内嵌空字符的选择器返回 `GRANIT_ERROR_INVALID_ARGUMENT`。

C++ 包装使用 `canvas_surface_desc::selector` 和 `surface::initialize_canvas`，默认 selector 同样为
`#canvas`。当前公共契约已经可编译，实际浏览器 Canvas 连接将在 WebGPU Renderer 后端接入；
在 Vulkan Renderer 上调用该入口始终明确返回不支持。

## 生命周期与归属

Surface 只能配合创建它的 Renderer 使用，跨 Renderer 销毁会返回
`GRANIT_ERROR_INVALID_HANDLE`。推荐在销毁 Renderer 前显式销毁全部 Surface；若仍有残留，
Renderer 会先销毁它们并使其句柄失效。不要让 Renderer 销毁与其 Surface 操作并发执行。

Surface 可以拥有多个 Swapchain；销毁 Surface 会先销毁全部所属 Swapchain 并使其句柄失效。
验证模式下，该操作会报告仍存活的 Swapchain，但不会把其借用 Backbuffer 展开成用户遗漏资源；
诊断不会阻止级联清理。
