// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "desktop_options.h"
#include "presentation_policy.h"
#include "sdl3_input.h"

#include "common/imgui_theme.h"
#include "model_viewer/application_core.h"
#include "model_viewer/frame_executor.h"
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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
#include <sstream>
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

constexpr std::string_view present_mode_name(granit::present_mode mode) noexcept {
  switch (mode) {
  case granit::present_mode::mailbox:
    return "mailbox";
  case granit::present_mode::immediate:
    return "immediate";
  default:
    return "fifo";
  }
}

constexpr std::string_view present_mode_label(granit::present_mode mode) noexcept {
  switch (mode) {
  case granit::present_mode::mailbox:
    return "Mailbox";
  case granit::present_mode::immediate:
    return "Immediate";
  default:
    return "FIFO";
  }
}

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
  if (result.ok()) {
    result = texture.write(
        premultiplied,
        {.bytes_per_row = static_cast<std::uint32_t>(width) * 4,
         .rows_per_image = static_cast<std::uint32_t>(height)},
        {.width = static_cast<std::uint32_t>(width), .height = static_cast<std::uint32_t>(height)});
  }
  if (result.ok())
    result = view.initialize(renderer, texture.native_handle());
  if (result.ok())
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

struct profile_metric {
  float p50{};
  float p95{};
  float p99{};
  std::size_t count{};
};

template <typename Selector, typename Filter>
profile_metric
summarize_profile(std::span<const granit::example::model_viewer::performance_sample> samples,
                  Selector selector, Filter filter) {
  std::vector<float> values;
  values.reserve(samples.size());
  for (const auto& sample : samples) {
    const auto value = selector(sample);
    if (filter(sample) && std::isfinite(value) && value >= 0.0F)
      values.push_back(value);
  }
  if (values.empty())
    return {};
  std::sort(values.begin(), values.end());
  const auto percentile = [&](std::size_t numerator) {
    const auto index = ((values.size() - 1) * numerator + 99) / 100;
    return values[index];
  };
  return {
      .p50 = percentile(50), .p95 = percentile(95), .p99 = percentile(99), .count = values.size()};
}

bool write_profile(const std::filesystem::path& path, const granit::renderer_info& renderer,
                   const granit::swapchain_info& swapchain, const std::filesystem::path& asset,
                   bool validation, bool ui,
                   std::span<const granit::example::model_viewer::performance_sample> samples) {
  const auto all = [](const auto&) { return true; };
  const auto gpu = [](const auto& sample) { return sample.gpu_timing_available; };
  const auto cpu =
      summarize_profile(samples, [](const auto& sample) { return sample.cpu_frame_ms; }, all);
  const auto slot =
      summarize_profile(samples, [](const auto& sample) { return sample.frame_slot_wait_ms; }, all);
  const auto present =
      summarize_profile(samples, [](const auto& sample) { return sample.present_wait_ms; }, all);
  const auto gpu_time =
      summarize_profile(samples, [](const auto& sample) { return sample.gpu_frame_ms; }, gpu);
  const auto write_metric = [](std::ostringstream& output, std::string_view name,
                               const profile_metric& metric, bool comma) {
    output << "    \"" << name << "\": {\"p50\": " << metric.p50 << ", \"p95\": " << metric.p95
           << ", \"p99\": " << metric.p99 << ", \"sample_count\": " << metric.count << '}'
           << (comma ? "," : "") << '\n';
  };
  std::ostringstream json;
  json << "{\n"
       << "  \"schema_version\": 1,\n"
       << "  \"backend\": "
       << std::quoted(renderer.backend == granit::renderer_backend::webgpu ? "webgpu" : "vulkan")
       << ",\n"
       << "  \"adapter\": " << std::quoted(renderer.adapter_name) << ",\n"
       << "  \"asset\": " << std::quoted(asset.generic_string()) << ",\n"
       << "  \"validation\": " << (validation ? "true" : "false") << ",\n"
       << "  \"width\": " << swapchain.width << ",\n"
       << "  \"height\": " << swapchain.height << ",\n"
       << "  \"present_mode\": " << std::quoted(present_mode_name(swapchain.presentation)) << ",\n"
       << "  \"ui\": " << (ui ? "true" : "false") << ",\n"
       << "  \"warmup_frames\": 300,\n"
       << "  \"sample_frames\": " << samples.size() << ",\n"
       << "  \"milliseconds\": {\n";
  write_metric(json, "cpu_frame", cpu, true);
  write_metric(json, "frame_slot_wait", slot, true);
  write_metric(json, "present_wait", present, true);
  write_metric(json, "gpu_frame", gpu_time, false);
  json << "  }\n}\n";

  std::error_code directory_error;
  if (path.has_parent_path())
    std::filesystem::create_directories(path.parent_path(), directory_error);
  if (directory_error)
    return false;
  std::ofstream output(path);
  output << json.str();
  return static_cast<bool>(output);
}

void print_usage() {
  std::cerr << "用法：granit_model_viewer_example --asset <文件> "
               "[--environment <文件.grenv>] "
               "[--backend=auto|vulkan] "
               "[--validation] [--smoke-test] [--no-ui] "
               "[--present-mode=fifo|immediate] [--profile-output <文件.json>]\n";
}

bool loading_needs_srgb_encoding(granit::texture_format format) noexcept {
  return format == granit::texture_format::rgba8_unorm ||
         format == granit::texture_format::bgra8_unorm;
}

granit::result render_loading_frame(granit::swapchain& swapchain,
                                    const granit::swapchain_info& swapchain_info,
                                    granit::frame_context& frame_context,
                                    granit::canvas_draw_list& canvas,
                                    granit::example::model_viewer::texture_registry& textures,
                                    const char* stage, float progress) {
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  const ImVec2 panel_size{420.0F, 118.0F};
  ImGui::SetNextWindowPos({(static_cast<float>(swapchain_info.width) - panel_size.x) * 0.5F,
                           (static_cast<float>(swapchain_info.height) - panel_size.y) * 0.5F});
  ImGui::SetNextWindowSize(panel_size);
  constexpr auto flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
  ImGui::Begin("Loading Model", nullptr, flags);
  ImGui::TextUnformatted(stage);
  ImGui::Spacing();
  ImGui::ProgressBar(progress, {-1.0F, 0.0F});
  ImGui::TextDisabled("The window remains responsive while large textures are decoded.");
  ImGui::End();
  ImGui::Render();

  auto result = canvas.clear();
  if (result.ok()) {
    result = granit::integration::imgui::append_draw_data(
        ImGui::GetDrawData(), canvas, granit::example::model_viewer::texture_registry::resolver,
        &textures);
  }
  granit::acquired_frame frame;
  if (result.ok())
    result = swapchain.acquire(frame);
  granit_texture backbuffer = GRANIT_NULL_HANDLE;
  granit_texture_view backbuffer_view = GRANIT_NULL_HANDLE;
  if (result.ok())
    result = swapchain.backbuffer(frame.image_index, backbuffer, backbuffer_view);
  granit::frame_recording recording;
  if (result.ok())
    result = frame_context.begin(frame, recording);
  if (result.ok()) {
    granit_canvas_record_desc record = GRANIT_CANVAS_RECORD_DESC_INIT;
    record.color = backbuffer_view;
    record.color_format = static_cast<granit_texture_format>(swapchain_info.format);
    record.width = swapchain_info.width;
    record.height = swapchain_info.height;
    record.load_operation = GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR;
    record.encode_srgb = loading_needs_srgb_encoding(swapchain_info.format) ? 1U : 0U;
    record.frame_slot = recording.frame_slot();
    result = canvas.record(recording.recorder().native_handle(), record);
  }
  if (result.ok())
    result = recording.submit();
  if (result.ok())
    result = swapchain.present(frame);
  if (result.failed()) {
    if (recording.valid())
      static_cast<void>(recording.abort());
    if (frame.valid())
      static_cast<void>(swapchain.cancel(frame));
  }
  return result;
}

struct gpu_upload_ui_context {
  granit::swapchain* swapchain{};
  granit::swapchain_info* swapchain_info{};
  granit::frame_context* frame_context{};
  granit::canvas_draw_list* canvas{};
  granit::example::model_viewer::texture_registry* textures{};
  granit::present_mode presentation{};
  int* pixel_width{};
  int* pixel_height{};
  granit::result result{granit::result::success};
  bool cancelled{};
};

bool update_gpu_upload_ui(const granit::example::model_viewer::gpu_scene_upload_progress& progress,
                          void* user_data) {
  auto& context = *static_cast<gpu_upload_ui_context*>(user_data);
  SDL_Event event{};
  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL3_ProcessEvent(&event);
    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
      context.cancelled = true;
    } else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
      *context.pixel_width = event.window.data1;
      *context.pixel_height = event.window.data2;
      if (*context.pixel_width > 0 && *context.pixel_height > 0) {
        context.result = context.swapchain->recreate(
            {.width = static_cast<std::uint32_t>(*context.pixel_width),
             .height = static_cast<std::uint32_t>(*context.pixel_height),
             .presentation = context.presentation});
        if (context.result.ok())
          context.result = context.swapchain->query_info(*context.swapchain_info);
      }
    }
  }
  if (context.cancelled || context.result.failed())
    return false;

  using enum granit::example::model_viewer::gpu_scene_upload_stage;
  const auto fraction = progress.total == 0 ? 1.0F
                                            : static_cast<float>(progress.completed) /
                                                  static_cast<float>(progress.total);
  const char* label = "Preparing GPU resources...";
  float value = 0.40F;
  switch (progress.stage) {
  case planning:
    value = 0.42F;
    break;
  case geometry:
    label = "Uploading vertices and indices...";
    value = 0.48F;
    break;
  case textures:
    label = "Uploading textures in batches...";
    value = 0.48F + fraction * 0.28F;
    break;
  case samplers:
    label = "Creating samplers...";
    value = 0.76F + fraction * 0.04F;
    break;
  case meshes:
    label = "Creating meshes...";
    value = 0.80F + fraction * 0.06F;
    break;
  case materials:
    label = "Creating materials...";
    value = 0.86F + fraction * 0.08F;
    break;
  }
  context.result =
      render_loading_frame(*context.swapchain, *context.swapchain_info, *context.frame_context,
                           *context.canvas, *context.textures, label, value);
  return context.result.ok();
}

struct desktop_frame_execution_context {
  granit::swapchain* swapchain{};
  granit::swapchain_info* swapchain_info{};
  granit::render_pipeline* pipeline{};
  granit_canvas_draw_list canvas{GRANIT_NULL_HANDLE};
};

granit::result execute_desktop_frame(
    granit::example::model_viewer::frame_packet&& packet,
    granit::example::model_viewer::frame_execution_result& output, void* user_data) {
  auto& context = *static_cast<desktop_frame_execution_context*>(user_data);
  granit::acquired_frame frame;
  const auto acquire_begin = std::chrono::steady_clock::now();
  auto result = context.swapchain->acquire(frame);
  output.acquire_wait_ms =
      std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - acquire_begin)
          .count();
  if (result.failed())
    return result;

  output.needs_recreate = frame.needs_recreate;
  granit_texture backbuffer = GRANIT_NULL_HANDLE;
  granit_texture_view backbuffer_view = GRANIT_NULL_HANDLE;
  result = context.swapchain->backbuffer(frame.image_index, backbuffer, backbuffer_view);
  if (result.ok()) {
    const auto render = packet.render_desc(
        backbuffer_view, static_cast<granit_texture_format>(context.swapchain_info->format),
        frame.handle, context.canvas);
    result = context.pipeline->render(render);
  }
  if (result.failed()) {
    const auto frame_result = result;
    static_cast<void>(context.swapchain->cancel(frame));
    return frame_result;
  }

  const auto present_begin = std::chrono::steady_clock::now();
  result = context.swapchain->present(frame);
  output.present_wait_ms =
      std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - present_begin)
          .count();
  output.needs_recreate = output.needs_recreate || frame.needs_recreate;
  return result;
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
  if (result.failed()) {
    print_usage();
    return 1;
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::cerr << "SDL3 初始化失败：" << SDL_GetError() << '\n';
    return 1;
  }
  sdl_quit quit;
  const auto initial_width = options.profile_output_path.empty() ? 1280 : 1920;
  const auto initial_height = options.profile_output_path.empty() ? 720 : 1080;
  std::unique_ptr<SDL_Window, window_deleter> window(
      SDL_CreateWindow("Granit Model Viewer", initial_width, initial_height,
                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY));
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
  granit::example::apply_imgui_theme();

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
  if (result.ok())
    result = granit::integration::sdl3::query_surface_type(window.get(), surface_type);
  if (result.ok()) {
    result = renderer.initialize({.application_name = "Granit Model Viewer",
                                  .enable_validation = options.enable_validation,
                                  .surface_types = surface_type,
                                  .backend = options.backend});
  }
  granit::renderer_info renderer_info;
  if (result.ok())
    result = renderer.get_info(renderer_info);
  granit::renderer_limits renderer_limits;
  if (result.ok())
    result = renderer.get_limits(renderer_limits);
  render_quality_config render_quality{
      .sample_count = renderer_limits.supports_sample_count(granit::sample_count::four)
                          ? GRANIT_SAMPLE_COUNT_4
                          : GRANIT_SAMPLE_COUNT_1,
      .enable_fxaa = true,
      .enable_specular_aa = true,
      .sampler_anisotropy = renderer_limits.max_sampler_anisotropy >= 8.0F ? 8.0F : 1.0F};
  if (result.ok())
    result = core.renderer_ready();

  if (result.ok())
    result =
        granit::integration::sdl3::create_surface(renderer.native_handle(), window.get(), surface);
  int pixel_width = 0;
  int pixel_height = 0;
  if (result.ok() && !SDL_GetWindowSizeInPixels(window.get(), &pixel_width, &pixel_height)) {
    result = granit::result::backend_unavailable;
  }
  if (result.ok() && !options.profile_output_path.empty() &&
      (pixel_width != 1920 || pixel_height != 1080)) {
    std::cerr << "性能采样要求窗口像素尺寸为 1920x1080，实际为 " << pixel_width << 'x'
              << pixel_height << '\n';
    result = granit::result::invalid_argument;
  }
  if (result.ok()) {
    result = swapchain.initialize(renderer.native_handle(), surface.native_handle(),
                                  {.width = static_cast<std::uint32_t>(pixel_width),
                                   .height = static_cast<std::uint32_t>(pixel_height),
                                   .presentation = options.presentation});
  }
  granit::swapchain_info swapchain_info;
  if (result.ok())
    result = swapchain.query_info(swapchain_info);
  desktop_frame_execution_context execution_context{&swapchain, &swapchain_info, &pipeline};
  inline_frame_executor frame_executor(execute_desktop_frame, &execution_context);
  if (result.ok() && !options.profile_output_path.empty() &&
      swapchain_info.presentation != options.presentation) {
    std::cerr << "性能采样要求的呈现模式不可用，后端回退到了其他模式\n";
    result = granit::result::unsupported;
  }

  granit::frame_context loading_frame_context;
  if (result.ok() && options.show_ui)
    result = loading_frame_context.initialize(renderer.native_handle());
  if (result.ok() && options.show_ui)
    result = upload_font_atlas(renderer.native_handle(), font_texture, font_view, font_sampler);
  ImTextureID font_texture_id = ImTextureID_Invalid;
  if (result.ok() && options.show_ui) {
    result = textures.register_texture(font_view.native_handle(), font_sampler.native_handle(),
                                       font_texture_id);
  }
  if (result.ok() && options.show_ui) {
    ImGui::GetIO().Fonts->SetTexID(font_texture_id);
    ImGui::GetIO().Fonts->TexRef._TexData->SetStatus(ImTextureStatus_OK);
    granit_canvas_draw_list_desc canvas_desc = GRANIT_CANVAS_DRAW_LIST_DESC_INIT;
    result = canvas.initialize(renderer.native_handle(), canvas_desc);
  }

  const std::filesystem::path asset_path(options.asset_path);
  std::vector<std::byte> environment_bytes;
  std::atomic<unsigned> loading_stage{0};
  std::future<granit::result> cpu_loading;
  if (result.ok()) {
    cpu_loading = std::async(std::launch::async, [&] {
      loading_stage.store(1, std::memory_order_release);
      std::vector<std::byte> asset_bytes;
      if (!read_file(asset_path, asset_bytes))
        return granit::result::invalid_argument;
      loading_stage.store(2, std::memory_order_release);
      if (!options.environment_path.empty() &&
          !read_file(options.environment_path, environment_bytes)) {
        return granit::result::invalid_argument;
      }
      loading_stage.store(3, std::memory_order_release);
      file_resolver resolver(asset_path.parent_path());
      const auto load_result = core.load_asset(asset_bytes, &resolver);
      loading_stage.store(4, std::memory_order_release);
      return load_result;
    });
  }
  bool loading_cancelled = false;
  while (result.ok() && cpu_loading.valid() &&
         cpu_loading.wait_for(std::chrono::milliseconds{0}) != std::future_status::ready) {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
        loading_cancelled = true;
      else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        pixel_width = event.window.data1;
        pixel_height = event.window.data2;
        if (pixel_width > 0 && pixel_height > 0) {
          result = swapchain.recreate({.width = static_cast<std::uint32_t>(pixel_width),
                                       .height = static_cast<std::uint32_t>(pixel_height),
                                       .presentation = options.presentation});
          if (result.ok())
            result = swapchain.query_info(swapchain_info);
        }
      }
    }
    if (result.failed() || loading_cancelled || !options.show_ui)
      break;
    const auto stage = loading_stage.load(std::memory_order_acquire);
    const char* label = stage <= 1   ? "Reading model file..."
                        : stage == 2 ? "Reading environment resources..."
                                     : "Parsing glTF and decoding textures...";
    const auto progress = stage <= 1 ? 0.10F : stage == 2 ? 0.20F : 0.35F;
    result = render_loading_frame(swapchain, swapchain_info, loading_frame_context, canvas,
                                  textures, label, progress);
    if (result == granit::result::out_of_date)
      result = granit::result::success;
    SDL_Delay(16);
  }
  if (cpu_loading.valid()) {
    const auto load_result = cpu_loading.get();
    if (result.ok())
      result = load_result;
  }
  if (result.ok() && loading_cancelled)
    result = granit::result::not_ready;
  if (result.ok() && options.show_ui)
    result = render_loading_frame(swapchain, swapchain_info, loading_frame_context, canvas,
                                  textures, "Preparing GPU upload...", 0.40F);
  gpu_upload_ui_context upload_ui{.swapchain = &swapchain,
                                  .swapchain_info = &swapchain_info,
                                  .frame_context = &loading_frame_context,
                                  .canvas = &canvas,
                                  .textures = &textures,
                                  .presentation = options.presentation,
                                  .pixel_width = &pixel_width,
                                  .pixel_height = &pixel_height};
  if (result.ok()) {
    result = core.upload(
        renderer.native_handle(), environment_bytes, render_quality.sampler_anisotropy,
        options.show_ui ? update_gpu_upload_ui : nullptr, options.show_ui ? &upload_ui : nullptr);
    if (result == granit::result::not_ready && upload_ui.result.failed())
      result = upload_ui.result;
  }
  if (result.ok() && options.show_ui)
    result = render_loading_frame(swapchain, swapchain_info, loading_frame_context, canvas,
                                  textures, "Creating render pipeline...", 0.96F);
  granit_render_pipeline_desc pipeline_desc = GRANIT_RENDER_PIPELINE_DESC_INIT;
  pipeline_desc.sample_count = render_quality.sample_count;
  pipeline_desc.enable_fxaa = render_quality.enable_fxaa;
  pipeline_desc.enable_specular_aa = render_quality.enable_specular_aa;
  if (result.ok())
    result = pipeline.initialize(renderer.native_handle(), pipeline_desc);
  bool gpu_metrics_enabled = false;
  if (result.ok()) {
    const auto metrics_result = pipeline.enable_metrics();
    if (metrics_result == granit::result::success)
      gpu_metrics_enabled = true;
    else if (metrics_result != granit::result::unsupported)
      result = metrics_result;
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
    if (preview_result.ok())
      preview_result = textures.register_texture(view, sampler, texture);
    if (preview_result.ok())
      previews.push_back({reference.image, reference.sampler, srgb, texture});
    return preview_result;
  };
  const auto rebuild_previews = [&]() {
    for (const auto& preview : previews)
      static_cast<void>(textures.unregister_texture(preview.texture));
    previews.clear();
    for (const auto& material : core.cpu_scene().materials) {
      granit::result preview_result;
      if ((preview_result = register_preview(material.base_color_texture, true)).failed() ||
          (preview_result = register_preview(material.emissive_texture, true)).failed() ||
          (preview_result = register_preview(material.metallic_roughness_texture, false))
              .failed() ||
          (preview_result = register_preview(material.normal_texture, false)).failed() ||
          (preview_result = register_preview(material.occlusion_texture, false)).failed())
        return preview_result;
    }
    return granit::result::success;
  };
  if (result.ok() && options.show_ui)
    result = rebuild_previews();
  if (result.ok() && options.show_ui)
    result = render_loading_frame(swapchain, swapchain_info, loading_frame_context, canvas,
                                  textures, "Loading complete", 1.0F);
  if (loading_frame_context.valid()) {
    const auto reset_result = loading_frame_context.reset();
    if (result.ok())
      result = reset_result;
  }

  if (result.failed()) {
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
  std::vector<performance_sample> profile_samples;
  if (!options.profile_output_path.empty())
    profile_samples.reserve(1000);
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
      if ((result = swapchain.reset()).failed() || (result = surface.reset()).failed() ||
          (result = granit::integration::sdl3::create_surface(renderer.native_handle(),
                                                              window.get(), surface))
              .failed() ||
          (result = swapchain.initialize(renderer.native_handle(), surface.native_handle(),
                                         {.width = static_cast<std::uint32_t>(pixel_width),
                                          .height = static_cast<std::uint32_t>(pixel_height),
                                          .presentation = options.presentation}))
              .failed() ||
          (result = swapchain.query_info(swapchain_info)).failed()) {
        break;
      }
      recreate_surface = false;
      recreate = false;
    }
    if (recreate) {
      result = swapchain.recreate({.width = static_cast<std::uint32_t>(pixel_width),
                                   .height = static_cast<std::uint32_t>(pixel_height),
                                   .presentation = options.presentation});
      if (result == granit::result::not_ready)
        continue;
      if (result.failed() || (result = swapchain.query_info(swapchain_info)).failed())
        break;
      recreate = false;
    }

    viewer_panel_changes changes;
    if (options.show_ui) {
      ImGui_ImplSDL3_NewFrame();
      ImGui::NewFrame();
      const renderer_panel_info panel_renderer{
          .backend = backend_name,
          .adapter = renderer_info.adapter_name,
          .swapchain_format = "Swapchain",
          .present_mode = present_mode_label(swapchain_info.presentation),
          .width = swapchain_info.width,
          .height = swapchain_info.height,
          .frame_slots = GRANIT_DEFAULT_FRAMES_IN_FLIGHT,
          .supported_sample_counts = renderer_limits.framebuffer_sample_counts,
          .max_sampler_anisotropy = renderer_limits.max_sampler_anisotropy};
      const performance_panel_info panel_performance{
          .frames_per_second = latest_sample.frames_per_second,
          .cpu_frame_ms = latest_sample.cpu_frame_ms,
          .frame_slot_wait_ms = latest_sample.frame_slot_wait_ms,
          .present_wait_ms = latest_sample.present_wait_ms,
          .gpu_frame_ms = latest_sample.gpu_frame_ms,
          .gpu_timing_available = latest_sample.gpu_timing_available,
          .history = core.performance().summarize()};
      changes = draw_viewer_panels(core.cpu_scene(), core.state(), panel_renderer,
                                   panel_performance, render_quality, previews);
      ImGui::Render();
      result = canvas.clear();
      if (result.ok()) {
        result = granit::integration::imgui::append_draw_data(
            ImGui::GetDrawData(), canvas, texture_registry::resolver, &textures);
      }
    }
    if (result.failed())
      break;

    if (changes.quality) {
      granit_render_pipeline_desc replacement_desc = GRANIT_RENDER_PIPELINE_DESC_INIT;
      replacement_desc.sample_count = changes.quality->sample_count;
      replacement_desc.enable_fxaa = changes.quality->enable_fxaa;
      replacement_desc.enable_specular_aa = changes.quality->enable_specular_aa;
      granit::render_pipeline replacement;
      result = replacement.initialize(renderer.native_handle(), replacement_desc);
      bool replacement_metrics_enabled = false;
      if (result.ok()) {
        const auto metrics_result = replacement.enable_metrics();
        if (metrics_result == granit::result::success)
          replacement_metrics_enabled = true;
        else if (metrics_result != granit::result::unsupported)
          result = metrics_result;
      }
      if (result.ok() && changes.quality->sampler_anisotropy != render_quality.sampler_anisotropy) {
        result = core.reupload_scene(renderer.native_handle(), changes.quality->sampler_anisotropy);
        if (result.ok() && options.show_ui)
          result = rebuild_previews();
      }
      if (result.ok()) {
        pipeline = std::move(replacement);
        render_quality = *changes.quality;
        gpu_metrics_enabled = replacement_metrics_enabled;
      }
    }
    if (result.failed())
      break;

    frame_packet tick_output;
    application_tick_input tick_input;
    tick_input.input = input_adapter.finish(options.show_ui && ImGui::GetIO().WantCaptureMouse,
                                            options.show_ui && ImGui::GetIO().WantCaptureKeyboard);
    tick_input.change = changes.state;
    tick_input.width = swapchain_info.width;
    tick_input.height = swapchain_info.height;
    if (has_pending_sample)
      tick_input.performance = latest_sample;
    if (result.ok())
      result = core.tick(tick_input, tick_output);
    if (result.ok()) {
      if (changes.material &&
          core.state().selected_material() != granit::example::gltf::invalid_index) {
        result = core.scene_gpu().update_material_factors(
            core.cpu_scene(), core.state().selected_material(), *changes.material);
      }
    }
    if (result.failed())
      break;
    execution_context.canvas =
        options.show_ui ? canvas.native_handle() : GRANIT_NULL_HANDLE;
    frame_execution_result execution;
    result = frame_executor.submit(std::move(tick_output), execution);
    recreate = recreate || execution.needs_recreate;
    const auto execution_action = desktop::classify_presentation_result(result);
    if (execution_action == desktop::presentation_action::retry) {
      result = granit::result::success;
      continue;
    }
    if (execution_action == desktop::presentation_action::recreate_swapchain) {
      result = granit::result::success;
      recreate = true;
      continue;
    }
    if (execution_action == desktop::presentation_action::recreate_surface) {
      result = granit::result::success;
      recreate_surface = true;
      continue;
    }
    if (execution_action == desktop::presentation_action::stop)
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
                     .frame_slot_wait_ms = execution.acquire_wait_ms,
                     .present_wait_ms = execution.present_wait_ms,
                     .gpu_frame_ms = gpu_frame_ms,
                     .gpu_timing_available = gpu_timing_available};
    has_pending_sample = true;
    if (!options.profile_output_path.empty() && rendered_frames >= 300)
      profile_samples.push_back(latest_sample);
    ++rendered_frames;
    if (!options.profile_output_path.empty() && profile_samples.size() >= 1000)
      break;
    if (options.profile_output_path.empty() && options.smoke_test && rendered_frames >= 3)
      break;
  }

  if (result.failed())
    std::cerr << "模型查看器帧循环失败：" << granit::result_message(result) << '\n';
  if (result.ok() && !options.profile_output_path.empty()) {
    if (profile_samples.size() != 1000 ||
        !write_profile(options.profile_output_path, renderer_info, swapchain_info,
                       options.asset_path, options.enable_validation, options.show_ui,
                       profile_samples)) {
      std::cerr << "写入模型查看器性能基线失败\n";
      result = granit::result::internal;
    } else {
      std::cout << "模型查看器性能基线已写入：" << options.profile_output_path << '\n';
    }
  }
  return result.failed() ? 1 : 0;
}
