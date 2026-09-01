// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "desktop_options.h"
#include "presentation_policy.h"
#include "sdl3_input.h"

#include "model_viewer/application_core.h"
#include "model_viewer/texture_registry.h"
#include "model_viewer/viewer_panels.h"

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>

#include <granit/granit.hpp>
#include <granit/integrations/imgui/renderer.hpp>
#include <granit/integrations/sdl3/surface.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>
#include <granit/pipeline/render_pipeline.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace {

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

granit::result upload_font_atlas(granit_renderer renderer, granit::texture& texture,
                                 granit::texture_view& view, granit::sampler& sampler) {
  unsigned char* pixels = nullptr;
  int width = 0;
  int height = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  if (pixels == nullptr || width <= 0 || height <= 0)
    return granit::result::internal;
  const auto pixel_count = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
  if (pixel_count > std::numeric_limits<std::size_t>::max() / 4)
    return granit::result::out_of_memory;
  std::vector<std::byte> premultiplied(static_cast<std::size_t>(pixel_count) * 4);
  for (std::size_t offset = 0; offset < premultiplied.size(); offset += 4) {
    const auto alpha = pixels[offset + 3];
    for (std::size_t channel = 0; channel < 3; ++channel) {
      premultiplied[offset + channel] = static_cast<std::byte>(
          (static_cast<std::uint32_t>(pixels[offset + channel]) * alpha + 127U) / 255U);
    }
    premultiplied[offset + 3] = static_cast<std::byte>(alpha);
  }
  auto result = texture.initialize(renderer, {.format = granit::texture_format::rgba8_unorm,
                                              .usage = granit::texture_usage::sampled |
                                                       granit::texture_usage::transfer_destination,
                                              .width = static_cast<std::uint32_t>(width),
                                              .height = static_cast<std::uint32_t>(height)});
  if (granit::succeeded(result)) {
    result = texture.write(
        premultiplied,
        {.bytes_per_row = static_cast<std::uint32_t>(width) * 4,
         .rows_per_image = static_cast<std::uint32_t>(height)},
        {.width = static_cast<std::uint32_t>(width), .height = static_cast<std::uint32_t>(height)});
  }
  if (granit::succeeded(result))
    result = view.initialize(renderer, texture.native_handle());
  if (granit::succeeded(result))
    result = sampler.initialize(renderer, {.address_u = granit::address_mode::clamp_to_edge,
                                           .address_v = granit::address_mode::clamp_to_edge,
                                           .address_w = granit::address_mode::clamp_to_edge});
  return result;
}

bool read_file(const std::filesystem::path& path, std::vector<std::byte>& output) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream)
    return false;
  const auto end = stream.tellg();
  if (end < 0 || static_cast<std::uintmax_t>(end) > std::numeric_limits<std::size_t>::max())
    return false;
  std::vector<std::byte> candidate(static_cast<std::size_t>(end));
  stream.seekg(0);
  if (!candidate.empty() &&
      !stream.read(reinterpret_cast<char*>(candidate.data()), static_cast<std::streamsize>(end))) {
    return false;
  }
  output = std::move(candidate);
  return true;
}

class file_resolver final : public granit::example::gltf::resource_resolver {
public:
  explicit file_resolver(std::filesystem::path base) : base_(std::move(base)) {}

  [[nodiscard]] bool resolve(std::string_view path, std::vector<std::byte>& bytes) const override {
    return read_file(base_ / std::filesystem::path(path), bytes);
  }

private:
  std::filesystem::path base_;
};

void print_usage() {
  std::cerr << "用法：granit_model_viewer_example --asset <文件> "
               "[--backend=auto|vulkan|webgpu] [--backend-library <文件>] "
               "[--validation] [--smoke-test]\n";
}

} // namespace

int main(int argc, char** argv) {
  using namespace granit::example::model_viewer;
  std::vector<std::string_view> arguments;
  arguments.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
  for (int index = 1; index < argc; ++index)
    arguments.emplace_back(argv[index]);
  desktop::options options;
  auto result = desktop::parse_options(arguments, options);
  if (granit::failed(result)) {
    print_usage();
    return 1;
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::cerr << "SDL3 初始化失败：" << SDL_GetError() << '\n';
    return 1;
  }
  sdl_quit quit;
  std::unique_ptr<SDL_Window, window_deleter> window(SDL_CreateWindow(
      "Granit Model Viewer", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY));
  if (!window) {
    std::cerr << "SDL3 窗口创建失败：" << SDL_GetError() << '\n';
    return 1;
  }
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  if (!ImGui_ImplSDL3_InitForOther(window.get())) {
    ImGui::DestroyContext();
    return 1;
  }
  imgui_quit imgui;
  ImGui::GetIO().IniFilename = nullptr;
  ImGui::StyleColorsDark();

  granit::renderer renderer;
  granit::surface surface;
  granit::swapchain swapchain;
  granit::render_pipeline pipeline;
  granit::texture font_texture;
  granit::texture_view font_view;
  granit::sampler font_sampler;
  granit::canvas_draw_list canvas;
  texture_registry textures;
  application_core core;
  result = core.begin_renderer();
  granit::surface_type surface_type{};
  if (granit::succeeded(result))
    result = granit::integration::sdl3::query_surface_type(window.get(), surface_type);
  if (granit::succeeded(result)) {
    result = renderer.initialize({.application_name = "Granit Model Viewer",
                                  .enable_validation = options.enable_validation,
                                  .surface_types = surface_type,
                                  .backend = options.backend,
                                  .backend_library_path = options.backend_library_path});
  }
  granit::renderer_info renderer_info;
  if (granit::succeeded(result))
    result = renderer.get_info(renderer_info);
  if (granit::succeeded(result))
    result = core.renderer_ready();

  if (granit::succeeded(result))
    result =
        granit::integration::sdl3::create_surface(renderer.native_handle(), window.get(), surface);
  int pixel_width = 0;
  int pixel_height = 0;
  if (granit::succeeded(result) &&
      !SDL_GetWindowSizeInPixels(window.get(), &pixel_width, &pixel_height)) {
    result = granit::result::backend_unavailable;
  }
  if (granit::succeeded(result)) {
    result = swapchain.initialize(renderer.native_handle(), surface.native_handle(),
                                  {.width = static_cast<std::uint32_t>(pixel_width),
                                   .height = static_cast<std::uint32_t>(pixel_height)});
  }
  granit::swapchain_info swapchain_info;
  if (granit::succeeded(result))
    result = swapchain.query_info(swapchain_info);

  const std::filesystem::path asset_path(options.asset_path);
  std::vector<std::byte> asset_bytes;
  if (granit::succeeded(result) && !read_file(asset_path, asset_bytes))
    result = granit::result::invalid_argument;
  file_resolver resolver(asset_path.parent_path());
  if (granit::succeeded(result))
    result = core.load_asset(asset_bytes, &resolver);
  if (granit::succeeded(result))
    result = core.upload(renderer.native_handle());
  const granit_render_pipeline_desc pipeline_desc = GRANIT_RENDER_PIPELINE_DESC_INIT;
  if (granit::succeeded(result))
    result = pipeline.initialize(renderer.native_handle(), pipeline_desc);
  bool gpu_metrics_enabled = false;
  if (granit::succeeded(result)) {
    const auto metrics_result = pipeline.enable_metrics();
    if (metrics_result == granit::result::success)
      gpu_metrics_enabled = true;
    else if (metrics_result != granit::result::unsupported)
      result = metrics_result;
  }
  if (granit::succeeded(result))
    result = upload_font_atlas(renderer.native_handle(), font_texture, font_view, font_sampler);
  ImTextureID font_texture_id = ImTextureID_Invalid;
  if (granit::succeeded(result)) {
    result = textures.register_texture(font_view.native_handle(), font_sampler.native_handle(),
                                       font_texture_id);
  }
  if (granit::succeeded(result)) {
    ImGui::GetIO().Fonts->SetTexID(font_texture_id);
    ImGui::GetIO().Fonts->TexRef._TexData->SetStatus(ImTextureStatus_OK);
    granit_canvas_draw_list_desc canvas_desc = GRANIT_CANVAS_DRAW_LIST_DESC_INIT;
    result = canvas.initialize(renderer.native_handle(), canvas_desc);
  }
  std::vector<texture_preview> previews;
  const auto register_preview = [&](const granit::example::gltf::texture_reference& reference,
                                    bool srgb) {
    if (reference.image == granit::example::gltf::invalid_index)
      return granit::result::success;
    ImTextureID existing = ImTextureID_Invalid;
    if (find_texture_preview(reference, srgb, previews, existing))
      return granit::result::success;
    granit_texture_view view = GRANIT_NULL_HANDLE;
    granit_sampler sampler = GRANIT_NULL_HANDLE;
    auto preview_result = core.scene_gpu().texture_binding(reference, srgb, view, sampler);
    ImTextureID texture = ImTextureID_Invalid;
    if (granit::succeeded(preview_result))
      preview_result = textures.register_texture(view, sampler, texture);
    if (granit::succeeded(preview_result))
      previews.push_back({reference.image, reference.sampler, srgb, texture});
    return preview_result;
  };
  if (granit::succeeded(result)) {
    for (const auto& material : core.cpu_scene().materials) {
      if (granit::failed(result = register_preview(material.base_color_texture, true)) ||
          granit::failed(result = register_preview(material.emissive_texture, true)) ||
          granit::failed(result = register_preview(material.metallic_roughness_texture, false)) ||
          granit::failed(result = register_preview(material.normal_texture, false)) ||
          granit::failed(result = register_preview(material.occlusion_texture, false)))
        break;
    }
  }

  if (granit::failed(result)) {
    std::cerr << "模型查看器初始化失败：" << granit::result_message(result);
    if (!core.diagnostic().empty())
      std::cerr << "（" << core.diagnostic() << "）";
    std::cerr << '\n';
    return 1;
  }
  const auto backend_name =
      renderer_info.backend == granit::renderer_backend::webgpu ? "WebGPU" : "Vulkan";
  const auto title =
      std::string("Granit Model Viewer | ") + backend_name + " | " + renderer_info.adapter_name;
  SDL_SetWindowTitle(window.get(), title.c_str());

  desktop::sdl3_input input_adapter;
  bool running = true;
  bool recreate = false;
  bool recreate_surface = false;
  std::uint32_t rendered_frames = 0;
  performance_sample latest_sample;
  bool has_pending_sample = false;
  while (running) {
    const auto cpu_begin = std::chrono::steady_clock::now();
    input_adapter.begin_frame();
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      input_adapter.process(event);
      if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
        running = false;
      else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        pixel_width = event.window.data1;
        pixel_height = event.window.data2;
        recreate = true;
      }
    }
    if (!running)
      break;
    if (pixel_width <= 0 || pixel_height <= 0) {
      SDL_Delay(16);
      continue;
    }
    if (recreate_surface) {
      if (granit::failed(result = swapchain.reset()) || granit::failed(result = surface.reset()) ||
          granit::failed(result = granit::integration::sdl3::create_surface(
                             renderer.native_handle(), window.get(), surface)) ||
          granit::failed(result = swapchain.initialize(
                             renderer.native_handle(), surface.native_handle(),
                             {.width = static_cast<std::uint32_t>(pixel_width),
                              .height = static_cast<std::uint32_t>(pixel_height)})) ||
          granit::failed(result = swapchain.query_info(swapchain_info))) {
        break;
      }
      recreate_surface = false;
      recreate = false;
    }
    if (recreate) {
      result = swapchain.recreate({.width = static_cast<std::uint32_t>(pixel_width),
                                   .height = static_cast<std::uint32_t>(pixel_height)});
      if (result == granit::result::not_ready)
        continue;
      if (granit::failed(result) || granit::failed(result = swapchain.query_info(swapchain_info)))
        break;
      recreate = false;
    }

    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    const renderer_panel_info panel_renderer{
        .backend = backend_name,
        .adapter = renderer_info.adapter_name,
        .swapchain_format = "Swapchain",
        .present_mode =
            swapchain_info.presentation == granit::present_mode::immediate ? "Immediate" : "FIFO",
        .width = swapchain_info.width,
        .height = swapchain_info.height,
        .frame_slots = GRANIT_DEFAULT_FRAMES_IN_FLIGHT};
    const performance_panel_info panel_performance{
        .frames_per_second = latest_sample.frames_per_second,
        .cpu_frame_ms = latest_sample.cpu_frame_ms,
        .frame_slot_wait_ms = latest_sample.frame_slot_wait_ms,
        .present_wait_ms = latest_sample.present_wait_ms,
        .gpu_frame_ms = latest_sample.gpu_frame_ms,
        .gpu_timing_available = latest_sample.gpu_timing_available,
        .history = core.performance().summarize()};
    const auto changes = draw_viewer_panels(core.cpu_scene(), core.state(), panel_renderer,
                                            panel_performance, previews);
    ImGui::Render();
    result = canvas.clear();
    if (granit::succeeded(result)) {
      result = granit::integration::imgui::append_draw_data(ImGui::GetDrawData(), canvas,
                                                            texture_registry::resolver, &textures);
    }
    if (granit::failed(result))
      break;

    granit::acquired_frame frame;
    const auto acquire_begin = std::chrono::steady_clock::now();
    result = swapchain.acquire(frame);
    const auto acquire_ms =
        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - acquire_begin)
            .count();
    const auto acquire_action = desktop::classify_presentation_result(result);
    if (acquire_action == desktop::presentation_action::retry) {
      result = granit::result::success;
      continue;
    }
    if (acquire_action == desktop::presentation_action::recreate_swapchain) {
      result = granit::result::success;
      recreate = true;
      continue;
    }
    if (acquire_action == desktop::presentation_action::recreate_surface) {
      result = granit::result::success;
      recreate_surface = true;
      continue;
    }
    if (acquire_action == desktop::presentation_action::stop)
      break;
    recreate = frame.needs_recreate;
    granit_texture backbuffer = GRANIT_NULL_HANDLE;
    granit_texture_view backbuffer_view = GRANIT_NULL_HANDLE;
    result = swapchain.backbuffer(frame.image_index, backbuffer, backbuffer_view);
    application_tick_output tick_output;
    application_tick_input tick_input;
    tick_input.input =
        input_adapter.finish(ImGui::GetIO().WantCaptureMouse, ImGui::GetIO().WantCaptureKeyboard);
    tick_input.change = changes.state;
    tick_input.width = swapchain_info.width;
    tick_input.height = swapchain_info.height;
    if (has_pending_sample)
      tick_input.performance = latest_sample;
    if (granit::succeeded(result))
      result = core.tick(tick_input, tick_output);
    if (granit::succeeded(result)) {
      if (changes.material &&
          core.state().selected_material() != granit::example::gltf::invalid_index) {
        result = core.scene_gpu().update_material_factors(
            core.cpu_scene(), core.state().selected_material(), *changes.material);
      }
    }
    if (granit::succeeded(result)) {
      tick_output.render.output = backbuffer_view;
      tick_output.render.output_format = static_cast<granit_texture_format>(swapchain_info.format);
      tick_output.render.frame = frame.handle;
      tick_output.render.canvas = canvas.native_handle();
      result = pipeline.render(tick_output.render);
    }
    if (granit::failed(result)) {
      const auto frame_result = result;
      static_cast<void>(swapchain.cancel(frame));
      result = frame_result;
      break;
    }
    const auto present_begin = std::chrono::steady_clock::now();
    result = swapchain.present(frame);
    const auto present_ms =
        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - present_begin)
            .count();
    recreate = recreate || frame.needs_recreate;
    const auto present_action = desktop::classify_presentation_result(result);
    if (present_action == desktop::presentation_action::retry) {
      result = granit::result::success;
      continue;
    }
    if (present_action == desktop::presentation_action::recreate_swapchain) {
      result = granit::result::success;
      recreate = true;
      continue;
    }
    if (present_action == desktop::presentation_action::recreate_surface) {
      result = granit::result::success;
      recreate_surface = true;
      continue;
    }
    if (present_action == desktop::presentation_action::stop)
      break;
    float gpu_frame_ms = 0.0F;
    bool gpu_timing_available = false;
    if (gpu_metrics_enabled) {
      granit_render_pipeline_metrics metrics = GRANIT_RENDER_PIPELINE_METRICS_INIT;
      const auto metrics_result = pipeline.get_metrics(metrics);
      if (metrics_result == granit::result::success) {
        gpu_frame_ms = static_cast<float>(metrics.total_gpu_ns) / 1'000'000.0F;
        gpu_timing_available = true;
      } else if (metrics_result == granit::result::unsupported) {
        gpu_metrics_enabled = false;
      }
    }
    const auto cpu_ms =
        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - cpu_begin)
            .count();
    latest_sample = {.frames_per_second = cpu_ms > 0.0F ? 1000.0F / cpu_ms : 0.0F,
                     .cpu_frame_ms = cpu_ms,
                     .frame_slot_wait_ms = acquire_ms,
                     .present_wait_ms = present_ms,
                     .gpu_frame_ms = gpu_frame_ms,
                     .gpu_timing_available = gpu_timing_available};
    has_pending_sample = true;
    ++rendered_frames;
    if (options.smoke_test && rendered_frames >= 3)
      break;
  }

  if (granit::failed(result))
    std::cerr << "模型查看器帧循环失败：" << granit::result_message(result) << '\n';
  return granit::failed(result) ? 1 : 0;
}
