<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Surface

## 定位

Surface 表示 Renderer 与窗口系统或浏览器 Canvas 之间的输出连接。普通头
`<granit/renderer/surface.h>` 提供句柄和销毁；Granit Window 用户直接调用
`granit_window_create_surface`。外部窗口和 Canvas 使用显式高级头
`<granit/renderer/native_surface.h>` 中的 `granit_surface_desc` 和 `granit_surface_create`，平台差异
只存在于描述的 `surface_type` 与 `source`。公共接口不暴露 Vulkan 或 WebGPU 类型。

Renderer 创建前设置 `granit_renderer_desc::presentation_mode = GRANIT_PRESENTATION_ENABLED`。
默认 `DISABLED` 只用于离屏渲染；未启用呈现或当前后端不支持的来源返回
`GRANIT_ERROR_UNSUPPORTED`。当前 Vulkan 后端支持平台窗口，浏览器 WebGPU 后端支持 Canvas。

## C API

Win32 示例：

```c
#include <granit/renderer/native_surface.h>

granit_renderer_desc renderer_desc = GRANIT_RENDERER_DESC_INIT;
renderer_desc.presentation_mode = GRANIT_PRESENTATION_ENABLED;

granit_surface_desc desc = GRANIT_SURFACE_DESC_INIT;
desc.surface_type = GRANIT_SURFACE_TYPE_WIN32_BIT;
desc.source.win32.instance = hinstance;
desc.source.win32.window = hwnd;

granit_surface surface = GRANIT_NULL_HANDLE;
granit_result result = granit_surface_create(renderer, &desc, &surface);
```

`surface_type` 必须恰好指定一个 `GRANIT_SURFACE_TYPE_*_BIT`。各来源字段为：

| 来源 | `surface_type` | `source` 字段 |
|---|---|---|
| Win32 | `GRANIT_SURFACE_TYPE_WIN32_BIT` | `win32.instance`、`win32.window` |
| XCB | `GRANIT_SURFACE_TYPE_XCB_BIT` | `xcb.connection`、`xcb.window` |
| Wayland | `GRANIT_SURFACE_TYPE_WAYLAND_BIT` | `wayland.display`、`wayland.surface` |
| Canvas | `GRANIT_SURFACE_TYPE_CANVAS_BIT` | `canvas.selector`、`canvas.selector_length` |

XCB 的 `window` 使用定宽 `uint32_t` 保存 `xcb_window_t`。公共头不包含 Windows、XCB 或 Wayland
平台头文件。Linux 构建可分别通过 `GRANIT_ENABLE_XCB` 和 `GRANIT_ENABLE_WAYLAND` 控制实现。

Canvas selector 是“指针 + UTF-8 字节长度”，不要求以空字符结尾。指针为空且长度为零时使用
`#canvas`；空选择器、超过 4096 字节或包含内嵌空字符时返回
`GRANIT_ERROR_INVALID_ARGUMENT`。Granit 仅在创建调用期间读取 selector。

## C++ API

`granit::surface_desc` 提供按来源命名的工厂，`granit::surface` 只有一个初始化入口：

```cpp
#include <granit/renderer/native_surface.hpp>

granit::surface surface;
auto result = surface.initialize(
    renderer,
    granit::surface_desc::win32(hinstance, hwnd));
```

其他来源使用 `surface_desc::xcb(connection, window)`、
`surface_desc::wayland(display, native_surface)` 或 `surface_desc::canvas("#viewport")`。
`granit::surface` 是无异常、move-only 的 RAII 类型，内部保存所属 Renderer 句柄。
Granit Window 的普通入口见[Window](window.md)。

## 生命周期与归属

Win32 窗口、XCB connection/window 和 Wayland display/surface 由调用方拥有，必须保持有效直到
Granit Surface 销毁。Granit 不取得这些原生对象的所有权。Canvas selector 的借用只持续到创建
调用返回。

Surface 只能配合创建它的 Renderer 使用，跨 Renderer 操作返回
`GRANIT_ERROR_INVALID_HANDLE`。Renderer 销毁时会销毁残留 Surface 并使句柄失效；调用方仍应按
Swapchain、Surface、原生窗口、Renderer 的依赖顺序安排销毁，且不要让 Renderer 销毁与 Surface
操作并发执行。

Surface 可以拥有多个 Swapchain。销毁 Surface 会先销毁全部所属 Swapchain 并使其句柄失效；
验证模式会报告仍存活的 Swapchain，但不会阻止级联清理。
