// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "desktop_options.h"
#include "sdl3_input.h"

#include "model_viewer/application_core.h"

#include <SDL3/SDL.h>

#include <granit/granit.hpp>
#include <granit/integrations/sdl3/surface.hpp>
#include <granit/pipeline/render_pipeline.hpp>

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

  granit::renderer renderer;
  granit::surface surface;
  granit::swapchain swapchain;
  granit::render_pipeline pipeline;
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
  std::uint32_t rendered_frames = 0;
  while (running) {
    input_adapter.begin_frame();
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
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
    if (recreate) {
      result = swapchain.recreate({.width = static_cast<std::uint32_t>(pixel_width),
                                   .height = static_cast<std::uint32_t>(pixel_height)});
      if (result == granit::result::not_ready)
        continue;
      if (granit::failed(result) || granit::failed(result = swapchain.query_info(swapchain_info)))
        break;
      recreate = false;
    }

    granit::acquired_frame frame;
    result = swapchain.acquire(frame);
    if (result == granit::result::out_of_date) {
      result = granit::result::success;
      recreate = true;
      continue;
    }
    if (granit::failed(result))
      break;
    recreate = frame.needs_recreate;
    granit_texture backbuffer = GRANIT_NULL_HANDLE;
    granit_texture_view backbuffer_view = GRANIT_NULL_HANDLE;
    result = swapchain.backbuffer(frame.image_index, backbuffer, backbuffer_view);
    application_tick_output tick_output;
    application_tick_input tick_input;
    tick_input.input = input_adapter.finish(false, false);
    tick_input.width = swapchain_info.width;
    tick_input.height = swapchain_info.height;
    if (granit::succeeded(result))
      result = core.tick(tick_input, tick_output);
    if (granit::succeeded(result)) {
      tick_output.render.output = backbuffer_view;
      tick_output.render.output_format = static_cast<granit_texture_format>(swapchain_info.format);
      tick_output.render.frame = frame.handle;
      result = pipeline.render(tick_output.render);
    }
    if (granit::succeeded(result))
      result = swapchain.present(frame);
    recreate = recreate || frame.needs_recreate;
    if (result == granit::result::out_of_date) {
      result = granit::result::success;
      recreate = true;
      continue;
    }
    if (granit::failed(result))
      break;
    ++rendered_frames;
    if (options.smoke_test && rendered_frames >= 3)
      break;
  }

  if (granit::failed(result))
    std::cerr << "模型查看器帧循环失败：" << granit::result_message(result) << '\n';
  return granit::failed(result) ? 1 : 0;
}
