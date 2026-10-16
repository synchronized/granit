<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# SDL3 与 ImGui Integration

## 当前状态

Granit 提供两个默认关闭的 C++20 可选集成组件：

| 构建选项 | 使用者目标 | 安装 component | 当前能力 |
|---|---|---|---|
| `GRANIT_BUILD_INTEGRATION_SDL3` | `granit::integration_sdl3` | `IntegrationSDL3` |
  SDL Window 到 Surface |
| `GRANIT_BUILD_INTEGRATION_IMGUI` | `granit::integration_imgui` | `IntegrationImGui` |
  Draw Data 到 Canvas；Window 无关的 ImGui 帧入口 |
| `GRANIT_BUILD_INTEGRATION_IMGUI` | `granit::integration_imgui_window` | `IntegrationImGuiWindow` |
  可选的 Granit Window/Input 到 ImGui 输入适配 |

启用组件前，父项目应已提供对应目标，或让 `find_package` 能找到 SDL3 3.2+ 与 ImGui。Granit 不会
默认下载或内置这些依赖。源码树开发时也可以显式启用锁定依赖：

```sh
cmake -S . -B build/integrations \
  -DGRANIT_BUILD_INTEGRATION_SDL3=ON \
  -DGRANIT_BUILD_INTEGRATION_IMGUI=ON \
  -DGRANIT_DEPENDENCY_POLICY=auto
```

当前锁定 SDL 3.4.10 与 ImGui 1.92.9。下载模式用于源码树编译、测试和示例验证；为避免把下载的
第三方目标混入 Granit 安装导出，该模式不安装 Integration 目标。需要安装 Integration component
时，应由父项目提供依赖目标，或安装可由 `find_package` 找到的依赖包。禁用两个 Integration 时
不会因此引入 SDL3 或 ImGui；SDL3 Window Backend 的独立依赖规则见[构建与安装](../guides/build.md)。

## SDL3 的两种接入方式

SDL3 Window Backend 与 `IntegrationSDL3` 解决不同的所有权场景：

| 场景 | 构建开关 | 窗口与事件所有者 | Granit 输入 |
|---|---|---|---|
| Granit 创建 SDL3 Window | `GRANIT_ENABLE_WINDOW_SDL3` | `granit::window_system` | 进入统一 Input API |
| 应用已有 `SDL_Window` | `GRANIT_BUILD_INTEGRATION_SDL3` | 应用和 SDL | 不转换，继续使用 SDL 事件 |

新应用若不需要直接访问 SDL API，优先使用 Window Backend；已有 SDL 生命周期、编辑器宿主或第三方
框架应保留 Integration 路径。两者可以在同一源码树构建中同时启用，但不能用 Integration 再包装
由 Granit SDL3 Window Backend 创建的窗口。

## SDL3 Surface

```cpp
#include <granit/integrations/sdl3/surface.hpp>

granit::renderer renderer;
auto result = renderer.initialize({.presentation = granit::presentation_mode::enabled});

granit::surface surface;
result = granit::integration::sdl3::create_surface(renderer, window, surface);
```

Integration 借用 SDL Window 的 Properties，不取得窗口或原生对象所有权。当前识别 `windows`、
`x11` 和 `wayland` 视频驱动：

- Win32 读取 `HINSTANCE` 与 `HWND`。
- Wayland 读取 `wl_display*` 与 `wl_surface*`。
- X11 在构建时找到 X11-xcb 后读取 `Display*` 和 X11 Window，并转换为 XCB connection；否则
  返回 `unsupported`。

SDL 继续负责事件循环。Wayland 窗口隐藏和再次显示后原生 Surface 可能变化，应用必须重新创建
Granit Surface 和 Swapchain。

## ImGui Draw Data

```cpp
#include <granit/integrations/imgui/renderer.hpp>

auto resolve_texture = [](ImTextureID id, granit_canvas_draw_state& state,
                          void* user_data) noexcept {
  /* 把 ImGui Texture ID 映射为 Granit Texture View 和 Sampler。 */
  return granit::result::success;
};

granit::integration::imgui::append_draw_data(ImGui::GetDrawData(), canvas, resolve_texture);
```

转换会处理 DisplayPos、FramebufferScale、顶点/索引偏移、裁剪矩形和多 Texture ID。Texture View
与 Sampler 仍由应用拥有；resolver 只在调用期间提供映射。`ImDrawCallback_ResetRenderState` 会被
忽略，其他用户 Draw Callback 当前返回 `unsupported`。

Integration 只承担 ImGui Renderer Backend 的 Draw Data 转换，不管理 ImGui Context、字体 Atlas、
输入队列或平台窗口。调用方负责创建/销毁 ImGui Context、上传字体 Atlas 并通过 resolver 映射
Texture ID。

## ImGui 帧与 Window 输入

只使用自己的事件循环、SDL3 或其他平台 Backend 时，应用可以只链接
`granit::integration_imgui`。该目标提供 Window 无关的 `frame_desc` 和 `begin_frame`：

```cpp
#include <granit/integrations/imgui/input.hpp>

granit::integration::imgui::begin_frame({
    .display_width = static_cast<float>(window_width),
    .display_height = static_cast<float>(window_height),
    .framebuffer_scale_x = scale_x,
    .framebuffer_scale_y = scale_y,
    .delta_seconds = delta_seconds,
});
```

`begin_frame` 在调用方编译执行，使用调用方当前的 ImGui Context；Context、字体和帧结束仍由
应用拥有。字体 Atlas 必须在第一次 `begin_frame` 前按应用的 Renderer Backend 完成构建。

使用 `granit::window` 的应用可额外链接 `granit::integration_imgui_window`，并将 Window/Input
事件直接转发给当前 ImGui Context：

```cpp
#include <granit/integrations/imgui/window_input.hpp>

granit::integration::imgui::window::process_window_event(window_event);
granit::integration::imgui::window::process_input_event(input_event);
```

推荐的每帧顺序是：先轮询并转发事件，再调用 `begin_frame`，构建 ImGui 界面，调用
`ImGui::Render`，最后调用 `append_draw_data`。Window 适配只转换键盘、修饰键、UTF-8 文本、
鼠标移动/按键/滚轮、指针离开和焦点事件，不订阅事件队列、不拥有 Window，也不处理 SDL3 外部
窗口。两个 ImGui 输入头文件中的逻辑在调用方内联执行，以避免共享库和应用各自拥有不同 ImGui
Context 的问题。

源码树的 `granit_tutorial_08_sdl_imgui` 展示外部宿主组合：ImGui 官方 SDL3 Platform Backend 处理输入，
应用把字体 Atlas 上传为 Granit Texture，Integration 转换 Draw Data，Canvas 录制到 Swapchain。
该示例需要锁定依赖模式，因为外部 ImGui 包不保证安装官方 backend 源文件。
