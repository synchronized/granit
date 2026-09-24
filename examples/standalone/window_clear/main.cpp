// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>
#include <granit/window.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <thread>

namespace {

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device || value == granit::result::unsupported;
}

granit::result render_clear_frame(granit::swapchain& swapchain,
                                  granit::frame_context& frame_context, std::uint32_t width,
                                  std::uint32_t height, bool& needs_recreate) {
  granit::acquired_frame frame;
  auto result = swapchain.acquire(frame);
  if (result.failed())
    return result;
  needs_recreate = frame.needs_recreate();

  granit::swapchain_backbuffer backbuffer;
  result = swapchain.backbuffer(frame, backbuffer);
  granit::frame_recording recording;
  if (result.ok())
    result = frame_context.begin(frame, recording);

  const granit::color_attachment_desc color{
      .view = backbuffer.view,
      .resolve_view = {},
      .clear_value = {.red = 0.04F, .green = 0.12F, .blue = 0.22F, .alpha = 1.0F}};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {0, 0, width, height}};
  if (result.ok())
    result = recording.recorder().begin_rendering(rendering);
  if (result.ok())
    result = recording.recorder().end_rendering();
  if (result.ok())
    result = recording.submit();
  if (result.ok())
    result = swapchain.present(frame);
  needs_recreate = needs_recreate || frame.needs_recreate();

  if (result.failed()) {
    if (recording.valid())
      static_cast<void>(recording.abort());
    if (frame.valid())
      static_cast<void>(swapchain.cancel(frame));
  }
  return result;
}

int report_failure(std::string_view operation, granit::result value, bool smoke_test) {
  if (smoke_test && environment_unavailable(value))
    return 77;
  std::cerr << operation << " failed: " << value.message() << '\n';
  return 1;
}

} // namespace

int main(int argument_count, char** arguments) {
  const bool smoke_test = argument_count == 2 && std::string_view{arguments[1]} == "--smoke-test";

  granit::window_system window_system;
  auto result = window_system.initialize();
  if (result.failed())
    return report_failure("window system initialization", result, smoke_test);

  granit::window window;
  result = window.initialize(window_system,
                             {.title = "Granit SDK Window Clear", .width = 800, .height = 600});
  if (result.failed())
    return report_failure("window creation", result, smoke_test);

  granit::renderer renderer;
  result = renderer.initialize({.application_name = "Granit SDK Window Clear",
                                .presentation = granit::presentation_mode::enabled});
  if (result.failed())
    return report_failure("renderer initialization", result, smoke_test);

  granit::surface surface;
  result = window.create_surface(renderer, surface);
  if (result.failed())
    return report_failure("surface creation", result, smoke_test);

  granit::window_state state;
  result = window.get_state(state);
  if (result.failed())
    return report_failure("window state query", result, smoke_test);

  granit::swapchain swapchain;
  result = swapchain.initialize(
      renderer, surface, {.width = state.framebuffer_width, .height = state.framebuffer_height});
  if (result.failed())
    return report_failure("swapchain creation", result, smoke_test);

  granit::frame_context frame_context;
  result = frame_context.initialize(renderer);
  if (result.failed())
    return report_failure("frame context creation", result, smoke_test);

  bool running = true;
  bool recreate_swapchain = false;
  bool recreate_surface = false;
  std::uint32_t rendered_frames = 0;
  while (running) {
    result = window_system.process_events();
    granit::window_event event;
    while (result.ok() && (result = window_system.poll(event)).ok()) {
      if (event.window != window.ref())
        continue;
      if (event.type == granit::window_event_type::close_requested)
        running = false;
      if (event.type == granit::window_event_type::resized ||
          event.type == granit::window_event_type::scale_changed)
        recreate_swapchain = true;
      if (event.type == granit::window_event_type::native_handle_changed)
        recreate_surface = true;
    }
    if (result == granit::result::not_ready)
      result = granit::result::success;
    if (result.failed() || !running)
      break;

    result = window.get_state(state);
    if (result.failed())
      break;
    if (state.framebuffer_width == 0 || state.framebuffer_height == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds{16});
      continue;
    }

    if (recreate_surface) {
      result = swapchain.reset();
      if (result.ok())
        result = surface.reset();
      if (result.ok())
        result = window.create_surface(renderer, surface);
      if (result.ok()) {
        result = swapchain.initialize(
            renderer, surface,
            {.width = state.framebuffer_width, .height = state.framebuffer_height});
      }
      recreate_surface = false;
      recreate_swapchain = false;
    } else if (recreate_swapchain) {
      result = swapchain.recreate(
          {.width = state.framebuffer_width, .height = state.framebuffer_height});
      recreate_swapchain = false;
    }
    if (result == granit::result::not_ready)
      continue;
    if (result.failed())
      break;

    bool frame_needs_recreate = false;
    result = render_clear_frame(swapchain, frame_context, state.framebuffer_width,
                                state.framebuffer_height, frame_needs_recreate);
    if (result == granit::result::out_of_date) {
      recreate_swapchain = true;
      continue;
    }
    if (result.failed())
      break;
    recreate_swapchain = recreate_swapchain || frame_needs_recreate;

    if (smoke_test && ++rendered_frames == 3)
      running = false;
  }

  if (result.failed())
    return report_failure("frame loop", result, smoke_test);
  return 0;
}
