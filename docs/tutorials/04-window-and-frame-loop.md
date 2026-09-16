<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 04：窗口与帧循环

本教程把前三篇中的 Renderer 和 Pipeline 接入可呈现窗口，介绍 Window System、Window、Surface、
Swapchain 和 Frame Context 的基本协作。完整的 SDL3/ImGui 应用位于
[`examples/samples/imgui`](../../examples/samples/imgui)；第三方窗口库的原生接入细节见
[SDL3 与 GLFW 窗口接入指南](../guides/window-library-integration.md)。

## 1. 创建 Window System 和 Window

Window component 管理平台窗口和事件队列，窗口与事件 API 必须在创建它的线程使用：

```cpp
#include <granit/granit.hpp>
#include <granit/window.hpp>

#include <span>

granit::window_system window_system;
check(window_system.initialize());

granit::window window;
check(window.initialize(window_system.native_handle(), {
    .title = "Granit Frame Loop",
    .width = 1280,
    .height = 720,
}));
```

Granit 不接管线程，也不替第三方窗口库分配或释放原生窗口。使用 SDL3、GLFW 或自定义窗口时，
应由对应 Integration 或 Native Surface API 提供 Surface 所需的原生值。

## 2. 创建可呈现 Renderer 和 Surface

Renderer 必须在创建 Surface 前启用呈现：

```cpp
granit::renderer renderer;
check(renderer.initialize({
    .application_name = "Granit Frame Loop",
    .presentation = granit::presentation_mode::enabled,
}));

granit::surface surface;
check(window.create_surface(renderer.native_handle(), surface));

granit::swapchain swapchain;
check(swapchain.initialize(renderer.native_handle(), surface.native_handle(), {
    .width = 1280,
    .height = 720,
    .presentation = granit::present_mode::fifo,
}));
```

Surface 借用 Window 的原生对象，不取得 Window 所有权。Window、Surface、Swapchain 和 Renderer
的当前契约分别见[Window 参考](../reference/window.md)、[Surface 参考](../reference/surface.md)
和[Swapchain 参考](../reference/swapchain.md)。

## 3. 处理事件和尺寸

每帧先处理平台事件，再读取 Window 状态。窗口最小化时 framebuffer 尺寸可能为零，应暂停获取
Swapchain 图像：

```cpp
check(window_system.process_events());
granit::window_event event;
while (window_system.poll(event) == granit::result::success) {
  if (event.type == GRANIT_WINDOW_EVENT_CLOSE_REQUESTED)
    running = false;
}

granit::window_state state;
check(window.get_state(state));
if (state.framebuffer_width == 0 || state.framebuffer_height == 0)
  continue;
```

收到 Resize、Scale 或 Native Handle Changed 事件后，应使用最新 framebuffer 像素尺寸重建
Swapchain。旧的原生窗口值不能跨隐藏、重建或平台后端切换继续使用。

## 4. 获取帧并提交

Frame Context 为每个在途帧槽管理 Command Recorder 和 GPU 完成边界：

```cpp
granit::frame_context frame_context;
check(frame_context.initialize(renderer.native_handle()));

granit::acquired_frame frame;
auto result = swapchain.acquire(frame);
if (result == granit::result::out_of_date || frame.needs_recreate)
  recreate = true;
else if (result.ok()) {
  granit::frame_recording recording;
  check(frame_context.begin(frame, recording));

  granit_texture backbuffer_texture = GRANIT_NULL_HANDLE;
  granit_texture_view backbuffer_view = GRANIT_NULL_HANDLE;
  check(swapchain.backbuffer(frame.image_index, backbuffer_texture, backbuffer_view));
  const granit::color_attachment_desc color{.view = backbuffer_view};
  const granit::rendering_desc rendering{
      .color_attachments = std::span{&color, 1},
      .area = {0, 0, state.framebuffer_width, state.framebuffer_height},
  };
  check(recording.recorder().begin_rendering(rendering));
  // 在这里绑定 Pipeline 并绘制；本例先只清除 Backbuffer。
  check(recording.recorder().end_rendering());
  check(recording.submit());
  result = swapchain.present(frame);
}
```

实际绘制时，`backbuffer_view` 应作为颜色附件传给 `begin_rendering()`；Pipeline、Viewport 和
Scissor 的设置方式与[Shader/Pipeline 教程](03-shader-and-pipeline.md)一致。提交失败时不要继续
Present 未完成的 Frame，应按返回结果取消或重建。

## 5. Resize 与恢复

以下条件都表示当前 Swapchain 不能继续直接使用：

- `acquire()` 或 `present()` 返回 `out_of_date`；
- Frame 标记 `needs_recreate`；
- Window 的 framebuffer 尺寸发生变化；
- 原生 Surface 发生变化。

恢复顺序是：停止当前帧、等待有效的非零 framebuffer 尺寸、调用 `swapchain.recreate()`，重新查询
Swapchain 信息，并按新格式或尺寸重建依赖 Backbuffer 的资源。详细平台差异见
[窗口接入指南](../guides/window-library-integration.md)。

## 6. 销毁顺序

停止帧循环后按以下顺序销毁：

```text
Frame Context / Pipeline 资源
  -> Swapchain
    -> Surface
      -> Renderer
        -> Window
          -> Window System
```

RAII 包装会在作用域结束时执行销毁，但作用域嵌套仍应保持父资源晚于子资源销毁。线程安全和回调
约束见[线程安全约定](../reference/thread-safety.md)。

## 运行完整示例

```powershell
cmake --preset windows-clang-release
cmake --build --preset windows-clang-release --target granit_sdl3_imgui_example
build/windows-clang-release/bin/granit_sdl3_imgui_example.exe --frames-in-flight 3
```

该示例还加入 ImGui、Canvas 和性能采样；本教程只保留窗口、呈现和帧循环的公共 API 主线。
