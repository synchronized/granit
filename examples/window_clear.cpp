// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>
#include <granit/window.hpp>

#include <cstdint>
#include <cstring>
#include <span>

namespace {

granit::result render_frame(granit::swapchain& swapchain, granit::frame_context& context,
                            std::uint32_t width, std::uint32_t height, bool& needs_recreate) {
  granit::acquired_frame frame;
  auto result = swapchain.acquire(frame);
  if (granit::failed(result))
    return result;
  needs_recreate = frame.needs_recreate;

  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  result = swapchain.backbuffer(frame.image_index, texture, view);
  granit::frame_recording recording;
  if (granit::succeeded(result))
    result = context.begin(frame, recording);
  auto& recorder = recording.recorder();
  const granit::color_attachment_desc color{
      .view = view, .clear_value = {.red = 0.04F, .green = 0.12F, .blue = 0.22F, .alpha = 1.0F}};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {0, 0, width, height}};
  if (granit::succeeded(result))
    result = recorder.begin_rendering(rendering);
  if (granit::succeeded(result))
    result = recorder.end_rendering();
  if (granit::succeeded(result))
    result = recording.submit();
  if (granit::succeeded(result))
    result = swapchain.present(frame);
  needs_recreate = needs_recreate || frame.needs_recreate;
  if (granit::failed(result)) {
    if (recording.valid())
      static_cast<void>(recording.abort());
    if (frame.valid())
      static_cast<void>(swapchain.cancel(frame));
  }
  return result;
}

} // namespace

int main(int argc, char** argv) {
  const bool smoke_test = argc > 1 && std::strcmp(argv[1], "--smoke-test") == 0;
  granit::window_system window_system;
  auto result = window_system.initialize({.backend = granit::window_backend::win32});
  granit::window app_window;
  if (granit::succeeded(result)) {
    result =
        app_window.initialize(window_system.native_handle(),
                              {.title = "Granit 窗口清屏",
                               .width = 800,
                               .height = 600,
                               .flags = GRANIT_WINDOW_VISIBLE_BIT | GRANIT_WINDOW_RESIZABLE_BIT |
                                        GRANIT_WINDOW_HIGH_DPI_BIT});
  }

  void* instance = nullptr;
  void* native_window = nullptr;
  if (granit::succeeded(result))
    result = app_window.native_win32(instance, native_window);

  granit::renderer renderer;
  if (granit::succeeded(result)) {
    result = renderer.initialize({.application_name = "Granit Window Clear",
                                  .enable_validation = true,
                                  .surface_types = granit::surface_type::win32});
  }
  granit::surface surface;
  if (granit::succeeded(result))
    result = surface.initialize_win32(renderer.native_handle(), {instance, native_window});

  std::uint32_t width = 800;
  std::uint32_t height = 600;
  granit::swapchain swapchain;
  if (granit::succeeded(result)) {
    result = swapchain.initialize(renderer.native_handle(), surface.native_handle(),
                                  {.width = width, .height = height});
  }
  granit::frame_context frame_context;
  if (granit::succeeded(result))
    result = frame_context.initialize(renderer.native_handle());

  bool running = granit::succeeded(result);
  bool recreate = false;
  std::uint32_t rendered_frames = 0;
  while (running) {
    granit::window_event event = GRANIT_WINDOW_EVENT_INIT;
    auto event_result = window_system.poll(event);
    while (event_result == granit::result::success) {
      if (event.type == GRANIT_WINDOW_EVENT_CLOSE_REQUESTED) {
        running = false;
      } else if (event.type == GRANIT_WINDOW_EVENT_RESIZED) {
        width = event.data.resized.width;
        height = event.data.resized.height;
        recreate = true;
      } else if (event.type == GRANIT_WINDOW_EVENT_SCALE_CHANGED) {
        width = event.data.scale.width;
        height = event.data.scale.height;
        recreate = true;
      }
      event = GRANIT_WINDOW_EVENT_INIT;
      event_result = window_system.poll(event);
    }
    if (event_result != granit::result::not_ready) {
      result = event_result;
      break;
    }
    if (!running)
      break;
    if (width == 0 || height == 0)
      continue;
    if (recreate) {
      result = swapchain.recreate({.width = width, .height = height});
      if (result == granit::result::not_ready)
        continue;
      if (granit::failed(result))
        break;
      recreate = false;
    }

    result = render_frame(swapchain, frame_context, width, height, recreate);
    if (result == granit::result::out_of_date) {
      result = granit::result::success;
      recreate = true;
      continue;
    }
    if (granit::failed(result))
      break;
    ++rendered_frames;
    if (smoke_test && rendered_frames >= 3)
      break;
  }

  return granit::failed(result) ? 1 : 0;
}
