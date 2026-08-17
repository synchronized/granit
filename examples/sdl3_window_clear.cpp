// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <SDL3/SDL.h>

#include <granit/granit.hpp>
#include <granit/integrations/sdl3/surface.hpp>

#include <cstdint>
#include <cstring>
#include <memory>
#include <span>

namespace {

struct sdl_quit {
  ~sdl_quit() { SDL_Quit(); }
};

struct window_deleter {
  void operator()(SDL_Window* window) const noexcept { SDL_DestroyWindow(window); }
};

granit::result render_frame(granit::swapchain& swapchain, granit::command_recorder& recorder,
                            std::uint32_t width, std::uint32_t height, bool& needs_recreate) {
  granit::acquired_frame frame;
  auto result = swapchain.acquire(frame);
  if (granit::failed(result))
    return result;
  needs_recreate = frame.needs_recreate;

  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  result = swapchain.backbuffer(frame.image_index, texture, view);
  if (granit::succeeded(result))
    result = recorder.begin();
  const granit::color_attachment_desc color{
      .view = view, .clear_value = {.red = 0.04F, .green = 0.12F, .blue = 0.22F, .alpha = 1.0F}};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {0, 0, width, height}};
  if (granit::succeeded(result))
    result = recorder.begin_rendering(rendering);
  if (granit::succeeded(result))
    result = recorder.end_rendering();
  if (granit::succeeded(result))
    result = recorder.end();
  if (granit::succeeded(result))
    result = recorder.submit(frame);
  if (granit::succeeded(result))
    result = swapchain.present(frame);
  needs_recreate = needs_recreate || frame.needs_recreate;
  const auto reset_result = recorder.reset();
  return granit::failed(result) ? result : reset_result;
}

} // namespace

int main(int argc, char** argv) {
  const bool smoke_test = argc > 1 && std::strcmp(argv[1], "--smoke-test") == 0;
  if (!SDL_Init(SDL_INIT_VIDEO))
    return 1;
  sdl_quit quit;
  std::unique_ptr<SDL_Window, window_deleter> window(
      SDL_CreateWindow("Granit SDL3 窗口清屏", 800, 600,
                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY));
  if (!window)
    return 1;

  granit::surface_type surface_type{};
  auto result = granit::integration::sdl3::query_surface_type(window.get(), surface_type);
  granit::renderer renderer;
  if (granit::succeeded(result)) {
    result = renderer.initialize({.application_name = "Granit SDL3 Window Clear",
                                  .enable_validation = true,
                                  .surface_types = surface_type});
  }
  granit::surface surface;
  if (granit::succeeded(result)) {
    result = granit::integration::sdl3::create_surface(renderer.native_handle(), window.get(),
                                                       surface);
  }

  int pixel_width = 0;
  int pixel_height = 0;
  if (granit::succeeded(result) &&
      !SDL_GetWindowSizeInPixels(window.get(), &pixel_width, &pixel_height)) {
    result = granit::result::backend_unavailable;
  }
  granit::swapchain swapchain;
  if (granit::succeeded(result)) {
    result = swapchain.initialize(renderer.native_handle(), surface.native_handle(),
                                  {.width = static_cast<std::uint32_t>(pixel_width),
                                   .height = static_cast<std::uint32_t>(pixel_height)});
  }
  granit::command_recorder recorder;
  if (granit::succeeded(result))
    result = recorder.initialize(renderer.native_handle());

  bool running = granit::succeeded(result);
  bool recreate = false;
  std::uint32_t rendered_frames = 0;
  while (running) {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        running = false;
      } else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        pixel_width = event.window.data1;
        pixel_height = event.window.data2;
        recreate = true;
      }
    }
    if (!running)
      break;
    if (pixel_width <= 0 || pixel_height <= 0)
      continue;
    if (recreate) {
      result = swapchain.recreate({.width = static_cast<std::uint32_t>(pixel_width),
                                   .height = static_cast<std::uint32_t>(pixel_height)});
      if (result == granit::result::not_ready)
        continue;
      if (granit::failed(result))
        break;
      recreate = false;
    }

    result = render_frame(swapchain, recorder, static_cast<std::uint32_t>(pixel_width),
                          static_cast<std::uint32_t>(pixel_height), recreate);
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
