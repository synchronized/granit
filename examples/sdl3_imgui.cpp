// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <SDL3/SDL.h>

#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>

#include <granit/granit.hpp>
#include <granit/integrations/imgui/renderer.hpp>
#include <granit/integrations/sdl3/surface.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <span>

namespace {

constexpr ImTextureID font_texture_id = 1;

struct sdl_quit {
  ~sdl_quit() { SDL_Quit(); }
};

struct window_deleter {
  void operator()(SDL_Window* window) const noexcept { SDL_DestroyWindow(window); }
};

struct imgui_quit {
  ~imgui_quit() {
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
  }
};

struct texture_binding {
  granit_texture_view view{GRANIT_NULL_HANDLE};
  granit_sampler sampler{GRANIT_NULL_HANDLE};
};

granit::result resolve_texture(ImTextureID texture, granit_canvas_draw_state& state,
                               void* user_data) noexcept {
  if (texture != font_texture_id || user_data == nullptr)
    return granit::result::invalid_argument;
  const auto& binding = *static_cast<const texture_binding*>(user_data);
  state.texture = binding.view;
  state.sampler = binding.sampler;
  return granit::result::success;
}

granit::result upload_font_atlas(granit_renderer renderer, granit::texture& texture,
                                 granit::texture_view& view, granit::sampler& sampler) {
  unsigned char* pixels = nullptr;
  int width = 0;
  int height = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  if (pixels == nullptr || width <= 0 || height <= 0)
    return granit::result::internal;

  auto result = texture.initialize(renderer, {.format = granit::texture_format::rgba8_unorm,
                                              .usage = granit::texture_usage::sampled |
                                                       granit::texture_usage::transfer_destination,
                                              .width = static_cast<std::uint32_t>(width),
                                              .height = static_cast<std::uint32_t>(height)});
  if (granit::succeeded(result)) {
    const auto byte_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    const granit::texture_data_layout layout{.bytes_per_row = static_cast<std::uint32_t>(width) * 4,
                                             .rows_per_image = static_cast<std::uint32_t>(height)};
    const granit::texture_write_region region{.width = static_cast<std::uint32_t>(width),
                                              .height = static_cast<std::uint32_t>(height)};
    result = texture.write(std::as_bytes(std::span{pixels, byte_count}), layout, region);
  }
  if (granit::succeeded(result))
    result = view.initialize(renderer, texture.native_handle());
  if (granit::succeeded(result)) {
    result = sampler.initialize(renderer, {.address_u = granit::address_mode::clamp_to_edge,
                                           .address_v = granit::address_mode::clamp_to_edge,
                                           .address_w = granit::address_mode::clamp_to_edge});
  }
  if (granit::succeeded(result)) {
    ImGui::GetIO().Fonts->SetTexID(font_texture_id);
    ImGui::GetIO().Fonts->TexRef._TexData->SetStatus(ImTextureStatus_OK);
  }
  return result;
}

bool needs_srgb_encoding(granit::texture_format format) {
  return format == granit::texture_format::rgba8_srgb ||
         format == granit::texture_format::bgra8_srgb;
}

granit::result render_frame(granit::swapchain& swapchain, granit::command_recorder& recorder,
                            granit::canvas_draw_list& canvas, const granit::swapchain_info& info,
                            bool& needs_recreate) {
  granit_canvas_draw_list_stats stats = GRANIT_CANVAS_DRAW_LIST_STATS_INIT;
  auto result = canvas.get_stats(stats);
  if (granit::failed(result))
    return result;
  const char* operation = "acquire";
  granit::acquired_frame frame;
  result = swapchain.acquire(frame);
  if (granit::failed(result))
    return result;
  needs_recreate = frame.needs_recreate;

  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  operation = "backbuffer";
  result = swapchain.backbuffer(frame.image_index, texture, view);
  if (granit::succeeded(result)) {
    operation = "recorder.begin";
    result = recorder.begin();
  }
  if (granit::succeeded(result) && stats.item_count == 0) {
    const granit::color_attachment_desc color{.view = view};
    const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                           .area = {0, 0, info.width, info.height}};
    operation = "recorder.begin_rendering";
    result = recorder.begin_rendering(rendering);
    if (granit::succeeded(result)) {
      operation = "recorder.end_rendering";
      result = recorder.end_rendering();
    }
  } else if (granit::succeeded(result)) {
    granit_canvas_record_desc record = GRANIT_CANVAS_RECORD_DESC_INIT;
    record.color = view;
    record.color_format = static_cast<granit_texture_format>(info.format);
    record.width = info.width;
    record.height = info.height;
    record.load_operation = GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR;
    record.encode_srgb = needs_srgb_encoding(info.format) ? 1U : 0U;
    operation = "canvas.record";
    result = canvas.record(recorder.native_handle(), record);
  }
  if (granit::succeeded(result)) {
    operation = "recorder.end";
    result = recorder.end();
  }
  if (granit::succeeded(result)) {
    operation = "recorder.submit";
    result = recorder.submit(frame);
  }
  if (granit::succeeded(result)) {
    operation = "swapchain.present";
    result = swapchain.present(frame);
  }
  needs_recreate = needs_recreate || frame.needs_recreate;
  const auto reset_result = recorder.reset();
  if (granit::failed(result))
    std::cerr << "帧阶段失败：" << operation << '\n';
  return granit::failed(result) ? result : reset_result;
}

} // namespace

int main(int argc, char** argv) {
  const bool smoke_test = argc > 1 && std::strcmp(argv[1], "--smoke-test") == 0;
  if (!SDL_Init(SDL_INIT_VIDEO))
    return 1;
  sdl_quit quit;
  std::unique_ptr<SDL_Window, window_deleter> window(SDL_CreateWindow(
      "Granit SDL3 + ImGui", 800, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY));
  if (!window)
    return 1;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  if (!ImGui_ImplSDL3_InitForVulkan(window.get())) {
    ImGui::DestroyContext();
    return 1;
  }
  imgui_quit imgui;
  ImGui::StyleColorsDark();
  ImGui::GetIO().IniFilename = nullptr;

  granit::surface_type surface_type{};
  auto result = granit::integration::sdl3::query_surface_type(window.get(), surface_type);
  granit::renderer renderer;
  if (granit::succeeded(result)) {
    result = renderer.initialize({.application_name = "Granit SDL3 ImGui",
                                  .enable_validation = true,
                                  .surface_types = surface_type});
  }
  granit::surface surface;
  if (granit::succeeded(result)) {
    result =
        granit::integration::sdl3::create_surface(renderer.native_handle(), window.get(), surface);
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
  granit::swapchain_info swapchain_info;
  if (granit::succeeded(result))
    result = swapchain.query_info(swapchain_info);
  granit::command_recorder recorder;
  if (granit::succeeded(result))
    result = recorder.initialize(renderer.native_handle());
  granit::canvas_draw_list canvas;
  granit_canvas_draw_list_desc canvas_desc = GRANIT_CANVAS_DRAW_LIST_DESC_INIT;
  if (granit::succeeded(result))
    result = canvas.initialize(renderer.native_handle(), canvas_desc);

  granit::texture font_texture;
  granit::texture_view font_view;
  granit::sampler font_sampler;
  if (granit::succeeded(result)) {
    result = upload_font_atlas(renderer.native_handle(), font_texture, font_view, font_sampler);
  }
  if (granit::failed(result))
    std::cerr << "SDL3 + ImGui 初始化失败，Granit 结果码：" << static_cast<int>(result) << '\n';
  texture_binding font_binding{font_view.native_handle(), font_sampler.native_handle()};

  bool running = granit::succeeded(result);
  bool recreate = false;
  std::uint32_t rendered_frames = 0;
  while (running) {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
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
      if (granit::failed(result) || granit::failed(result = swapchain.query_info(swapchain_info))) {
        break;
      }
      recreate = false;
    }

    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Granit Integration");
    ImGui::TextUnformatted("SDL3 owns the window and input; Granit Canvas renders ImGui.");
    ImGui::Text("Framebuffer: %u x %u", swapchain_info.width, swapchain_info.height);
    ImGui::End();
    ImGui::Render();
    result = canvas.clear();
    if (granit::succeeded(result)) {
      result = granit::integration::imgui::append_draw_data(ImGui::GetDrawData(), canvas,
                                                            resolve_texture, &font_binding);
      if (granit::failed(result))
        std::cerr << "ImGui Draw Data 转换失败，Granit 结果码：" << static_cast<int>(result)
                  << '\n';
    }
    if (granit::succeeded(result)) {
      result = render_frame(swapchain, recorder, canvas, swapchain_info, recreate);
      if (granit::failed(result) && result != granit::result::out_of_date)
        std::cerr << "ImGui Canvas 录制或呈现失败，Granit 结果码：" << static_cast<int>(result)
                  << '\n';
    }
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

  if (granit::failed(result))
    std::cerr << "SDL3 + ImGui 帧循环失败，Granit 结果码：" << static_cast<int>(result) << '\n';
  return granit::failed(result) ? 1 : 0;
}
