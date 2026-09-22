// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/frame_context.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/surface.hpp>
#include <granit/renderer/swapchain.hpp>
#include <granit/window.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <thread>

namespace {

int report_failure(std::string_view operation, granit::result result) {
  std::cerr << operation << " failed: " << result.message() << "\n";
  return 1;
}

granit::result poll_window_events(granit::window_system& window_system, bool& running,
                                  bool& recreate) {
  granit::window_event event;
  granit::result result;
  while ((result = window_system.poll(event)).ok()) {
    if (event.type == granit::window_event_type::close_requested)
      running = false;
    if (event.type == granit::window_event_type::resized ||
        event.type == granit::window_event_type::scale_changed ||
        event.type == granit::window_event_type::native_handle_changed) {
      recreate = true;
    }
  }
  return result == granit::result::not_ready ? granit::result::success : result;
}

granit::result poll_input_events(granit::window_system& window_system, bool& running) {
  granit::input_event event;
  granit::result result;
  while ((result = window_system.poll(event)).ok()) {
    if (event.type == granit::input_event_type::key &&
        event.data.key.action == granit::key_action::released &&
        event.data.key.physical == granit::physical_key::escape) {
      running = false;
    }
  }
  return result == granit::result::not_ready ? granit::result::success : result;
}

granit::result render_clear_frame(granit::swapchain& swapchain, granit::frame_context& context,
                                  const granit::swapchain_info& info, bool& needs_recreate) {
  granit::acquired_frame frame;
  auto result = swapchain.acquire(frame);
  if (result.failed())
    return result;
  needs_recreate = needs_recreate || frame.needs_recreate;

  granit::swapchain_backbuffer backbuffer;
  result = swapchain.backbuffer(frame, backbuffer);

  granit::frame_recording recording;
  if (result.ok())
    result = context.begin(frame, recording);

  const granit::color_attachment_desc color{
      .view = backbuffer.view,
      .resolve_view = {},
      .clear_value = {.red = 0.04F, .green = 0.03F, .blue = 0.05F, .alpha = 1.0F}};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {0, 0, info.width, info.height}};
  if (result.ok())
    result = recording.recorder().begin_rendering(rendering);
  if (result.ok())
    result = recording.recorder().end_rendering();
  if (result.ok())
    result = recording.submit();
  if (result.ok())
    result = swapchain.present(frame);

  needs_recreate = needs_recreate || frame.needs_recreate;
  if (result.failed()) {
    if (recording.valid())
      static_cast<void>(recording.abort());
    if (frame.valid())
      static_cast<void>(swapchain.cancel(frame));
  }
  return result;
}

} // namespace

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
  result = window.initialize(window_system,
                             {.title = application_name, .width = width, .height = height});
  if (result.failed()) {
    return report_failure("window initialize", result);
  }

  granit::window_state window_state;
  result = window.get_state(window_state);
  if (result.failed()) {
    return report_failure("window state query", result);
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
  result = window.create_surface(renderer, surface);
  if (result.failed()) {
    return report_failure("surface initialize", result);
  }

  // 创建交换链。
  granit::swapchain swapchain;
  result = swapchain.initialize(renderer, surface,
                                {.width = window_state.framebuffer_width,
                                 .height = window_state.framebuffer_height});
  if (result.failed()) {
    return report_failure("swapchain initialize", result);
  }

  granit::swapchain_info swapchain_info;
  result = swapchain.query_info(swapchain_info);
  if (result.failed()) {
    return report_failure("swapchain info query", result);
  }

  // 创建帧上下文。
  granit::frame_context context;
  result = context.initialize(renderer);
  if (result.failed()) {
    return report_failure("frame context initialize", result);
  }

  bool running = true;
  bool recreate = false;
  while (running) {
    if (result = window_system.process_events(); result.failed()) {
      return report_failure("window system process events", result);
    }

    result = poll_window_events(window_system, running, recreate);
    if (result.failed())
      return report_failure("window event poll", result);

    result = poll_input_events(window_system, running);
    if (result.failed())
      return report_failure("input event poll", result);

    if (!running)
      break;

    // 处理渲染事件。
    if (result = renderer.process_events(); result.failed()) {
      return report_failure("renderer process events", result);
    }

    result = window.get_state(window_state);
    if (result.failed()) {
      return report_failure("window state query", result);
    }
    const auto framebuffer_width = window_state.framebuffer_width;
    const auto framebuffer_height = window_state.framebuffer_height;
    if (framebuffer_width == 0 || framebuffer_height == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds{16});
      continue;
    }

    if (recreate || framebuffer_width != swapchain_info.width ||
        framebuffer_height != swapchain_info.height) {
      result = swapchain.recreate({.width = framebuffer_width, .height = framebuffer_height});
      if (result == granit::result::not_ready) {
        std::this_thread::sleep_for(std::chrono::milliseconds{16});
        continue;
      }
      if (result.failed()) {
        return report_failure("swapchain recreate", result);
      }
      result = swapchain.query_info(swapchain_info);
      if (result.failed()) {
        return report_failure("swapchain info query", result);
      }
      recreate = false;
    }

    result = render_clear_frame(swapchain, context, swapchain_info, recreate);
    if (result == granit::result::out_of_date) {
      recreate = true;
      continue;
    }
    if (result.failed())
      return report_failure("render clear frame", result);
  }
}
