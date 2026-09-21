// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/frame_context.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/swapchain.hpp>
#include <granit/window.hpp>

#include <iostream>

int report_failure(std::string_view operation, granit::result result) {
  std::cerr << operation << " failed: " << result.message() << "\n";
  return 1;
}

int main() {
  // 创建窗口系统。
  granit::window_system window_system;
  granit::result result = window_system.initialize();
  if (result.failed()) {
    return report_failure("window system initialize", result);
  }
  const std::string_view application_name = "Granit Tutorial 01";
  constexpr std::uint32_t width = 1280;
  constexpr std::uint32_t height = 720;

  // 创建窗口。
  granit::window window;
  result = window.initialize(window_system.native_handle(),
                             {.title = application_name, .width = width, .height = height});
  if (result.failed()) {
    return report_failure("window initialize", result);
  }

  // 创建渲染器。
  granit::renderer renderer;
  result = renderer.initialize(
      {.application_name = application_name, .presentation = granit::presentation_mode::enabled});
  if (result.failed()) {
    return report_failure("renderer initialize", result);
  }

  // 创建表面。
  granit::surface surface;
  result = window.create_surface(renderer.native_handle(), surface);
  if (result.failed()) {
    return report_failure("surface initialize", result);
  }

  // 创建交换链。
  granit::swapchain swapchain;
  result = swapchain.initialize(renderer.native_handle(), surface.native_handle(),
                                {.width = width, .height = height});
  if (result.failed()) {
    return report_failure("swapchain initialize", result);
  }

  // 创建帧上下文。
  granit::frame_context context;
  result = context.initialize(renderer.native_handle());
  if (result.failed()) {
    return report_failure("frame context initialize", result);
  }

  bool running = true;
  while (running) {
    if (result = window_system.process_events(); result.failed()) {
      return report_failure("window system process events", result);
    }

    {
      // 处理窗口事件。
      granit::window_event event;
      while (window_system.poll(event).ok()) {
        // 处理关闭、尺寸、焦点与缩放。
        if (event.type == granit::window_event_type::close_requested)
          running = false;
      }
    }

    {
      // 处理输入事件。
      granit::input_event event;
      while (window_system.poll(event).ok()) {
        // 处理键盘、文本与指针变化。
        if (event.type == granit::input_event_type::key) {
          if (event.data.key.action == granit::key_action::released &&
              event.data.key.physical == granit::physical_key::escape) {
            running = false;
          }
        }
      }
    }

    // 处理渲染事件。
    if (result = renderer.process_events(); result.failed()) {
      return report_failure("renderer process events", result);
    }

    // 获取帧。
    granit::acquired_frame frame;
    if (result = swapchain.acquire(frame); result.failed()) {
      return report_failure("swapchain acquire frame", result);
    }

    // 获取 Backbuffer。
    granit_texture backbuffer = GRANIT_NULL_HANDLE;
    granit_texture_view backbuffer_view = GRANIT_NULL_HANDLE;
    result = swapchain.backbuffer(frame.image_index, backbuffer, backbuffer_view);
    if (result.failed()) {
      return report_failure("swapchain backbuffer acquire frame", result);
    }

    granit::frame_recording recording;
    result = context.begin(frame, recording);
    if (result.failed()) {
      return report_failure("frame context begin", result);
    }

    const granit::color_attachment_desc color{
        .view = backbuffer_view,
        .clear_value = {.red = 0.04F, .green = 0.03F, .blue = 0.05F, .alpha = 1.0F}};
    const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                           .area = {0, 0, width, height}};
    result = recording.recorder().begin_rendering(rendering);
    if (result.failed()) {
      return report_failure("recording recorder begin rendering", result);
    }

    result = recording.recorder().end_rendering();
    if (result.failed()) {
      return report_failure("recording recorder end rendering", result);
    }

    if (result = recording.submit(); result.failed()) {
      return report_failure("frame recorder submit", result);
    }
    if (result = swapchain.present(frame); result.failed()) {
      return report_failure("swapchain present", result);
    }
  }
}
