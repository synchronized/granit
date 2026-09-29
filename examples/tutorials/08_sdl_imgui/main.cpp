// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <SDL3/SDL.h>

#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>

#include "imgui/imgui_theme.h"
#include "sdl/sdl3_lifecycle.h"
#include "tutorials/08_sdl_imgui/content.h"
#include "tutorials/08_sdl_imgui/resources.h"

#include <granit/granit.hpp>
#include <granit/integrations/imgui/renderer.hpp>
#include <granit/integrations/sdl3/surface.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>

namespace {

granit::result render_frame(granit::swapchain& swapchain, granit::frame_context& frame_context,
                            granit::canvas_draw_list& canvas, const granit::swapchain_info& info,
                            bool& needs_recreate) {
  granit::acquired_frame frame;
  auto result = swapchain.acquire(frame);
  if (result.failed())
    return result;
  needs_recreate = frame.needs_recreate();

  granit::swapchain_backbuffer backbuffer;
  if (result.ok())
    result = swapchain.backbuffer(frame, backbuffer);

  granit::frame_recording recording;
  if (result.ok())
    result = frame_context.begin(frame, recording);
  if (result.ok()) {
    result = granit::example::record_imgui_sample_canvas(
        recording.recorder(), canvas, backbuffer.view, info, recording.frame_slot());
  }
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

} // namespace

int main(int argc, char** argv) {
  bool smoke_test = false;
  for (int index = 1; index < argc; ++index) {
    if (std::strcmp(argv[index], "--smoke-test") == 0)
      smoke_test = true;
    else
      return 1;
  }

  if (!SDL_Init(SDL_INIT_VIDEO))
    return 1;
  granit::example::sdl::sdl_quit sdl;
  std::unique_ptr<SDL_Window, granit::example::sdl::window_deleter> window(SDL_CreateWindow(
      "Granit 08 SDL3 + ImGui", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY));
  if (!window)
    return 1;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  if (!ImGui_ImplSDL3_InitForVulkan(window.get())) {
    ImGui::DestroyContext();
    return 1;
  }
  granit::example::sdl::imgui_quit imgui;
  granit::example::apply_imgui_theme();
  ImGui::GetIO().IniFilename = nullptr;

  granit::renderer renderer;
  auto result = renderer.initialize({.application_name = "Granit SDL3 ImGui Tutorial",
                                     .presentation = granit::presentation_mode::enabled,
                                     .frames_in_flight = 2});
  granit::surface surface;
  if (result.ok())
    result = granit::integration::sdl3::create_surface(renderer, window.get(), surface);

  int pixel_width{};
  int pixel_height{};
  if (result.ok() && !SDL_GetWindowSizeInPixels(window.get(), &pixel_width, &pixel_height))
    result = granit::result::backend_unavailable;

  granit::swapchain swapchain;
  if (result.ok()) {
    result = swapchain.initialize(renderer, surface,
                                  {.width = static_cast<std::uint32_t>(pixel_width),
                                   .height = static_cast<std::uint32_t>(pixel_height),
                                   .minimum_image_count = 2,
                                   .presentation = granit::present_mode::fifo});
  }
  granit::swapchain_info swapchain_info;
  if (result.ok())
    result = swapchain.query_info(swapchain_info);

  granit::frame_context frame_context;
  if (result.ok())
    result = frame_context.initialize(renderer);
  granit::canvas_draw_list canvas;
  if (result.ok())
    result = canvas.initialize(renderer, {.frame_slot_count = 2});

  granit::texture font_texture;
  granit::texture_view font_view;
  granit::texture checker_texture;
  granit::texture_view checker_view;
  granit::sampler sampler;
  if (result.ok()) {
    result = granit::example::upload_imgui_font_atlas(renderer, font_texture, font_view, sampler);
  }
  if (result.ok()) {
    result = granit::example::upload_imgui_checker_texture(renderer, checker_texture, checker_view);
  }
  if (result.failed()) {
    std::cerr << "SDL3 + ImGui 初始化失败，Granit 结果码：" << static_cast<int>(result) << '\n';
    return 1;
  }

  granit::example::imgui_sample_texture_bindings bindings{
      .font = {font_view.ref(), sampler.ref()}, .checker = {checker_view.ref(), sampler.ref()}};
  bool running = true;
  bool needs_recreate = false;
  std::uint32_t rendered_frames{};
  while (running) {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        running = false;
      } else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        pixel_width = event.window.data1;
        pixel_height = event.window.data2;
        needs_recreate = true;
      }
    }
    if (!running)
      break;
    if (pixel_width <= 0 || pixel_height <= 0)
      continue;

    if (needs_recreate) {
      result = swapchain.recreate({.width = static_cast<std::uint32_t>(pixel_width),
                                   .height = static_cast<std::uint32_t>(pixel_height),
                                   .minimum_image_count = 2,
                                   .presentation = granit::present_mode::fifo});
      if (result == granit::result::not_ready)
        continue;
      if (result.failed() || (result = swapchain.query_info(swapchain_info)).failed())
        break;
      needs_recreate = false;
    }

    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    granit::example::build_imgui_sample(
        {.framebuffer_width = swapchain_info.width,
         .framebuffer_height = swapchain_info.height,
         .presentation = "FIFO",
         .show_custom_texture = true,
         .custom_texture = granit::example::imgui_checker_texture_id});
    ImGui::Render();

    result = canvas.clear();
    if (result.ok()) {
      result = granit::integration::imgui::append_draw_data(
          ImGui::GetDrawData(), canvas, granit::example::resolve_imgui_sample_texture, &bindings);
    }
    if (result.ok())
      result = render_frame(swapchain, frame_context, canvas, swapchain_info, needs_recreate);
    if (result == granit::result::out_of_date) {
      result = granit::result::success;
      needs_recreate = true;
      continue;
    }
    if (result.failed())
      break;

    ++rendered_frames;
    if (smoke_test && rendered_frames >= 3)
      break;
  }

  if (result.failed()) {
    std::cerr << "SDL3 + ImGui 帧循环失败，Granit 结果码：" << static_cast<int>(result) << '\n';
  }
  return result.failed() ? 1 : 0;
}
