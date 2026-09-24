// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/desktop/application.h"
#include "model_viewer/desktop/threaded_render_service.h"

#include "assets/asset_system.h"
#include "model_viewer/presentation_recovery.h"
#include "model_viewer/render_task_executor.h"
#include "model_viewer/viewer_frame_builder.h"
#include "model_viewer/viewer_input_accumulator.h"
#include "model_viewer/viewer_panels.h"
#include "model_viewer/viewer_session.h"
#include "model_viewer/viewer_texture_previews.h"
#include "model_viewer/viewer_ui.h"

#include <imgui.h>

#include <granit/granit.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>
#include <granit/pipeline/render_pipeline.hpp>
#include <granit/window.hpp>

#include <algorithm>
#include <array>
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
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct desktop_window_events {
  bool close_requested{};
  bool resized{};
};

granit::result pump_window_events(
    granit::window_system& system, granit::window& window, granit::window_state& state,
    desktop_window_events& output, granit::example::model_viewer::viewer_ui* ui = nullptr,
    granit::example::model_viewer::viewer_input_accumulator* input = nullptr) noexcept {
  output = {};
  auto result = system.process_events();
  granit::window_event window_event;
  while (result.ok()) {
    const auto poll_result = system.poll(window_event);
    if (poll_result == granit::result::not_ready)
      break;
    if (poll_result.failed())
      return poll_result;
    if (window_event.window != window.ref())
      continue;
    if (ui != nullptr)
      ui->process(window_event);
    if (input != nullptr)
      input->process(window_event);
    output.close_requested =
        output.close_requested || window_event.type == granit::window_event_type::close_requested;
    output.resized = output.resized || window_event.type == granit::window_event_type::resized ||
                     window_event.type == granit::window_event_type::scale_changed;
  }
  granit::input_event input_event;
  while (result.ok()) {
    const auto poll_result = system.poll(input_event);
    if (poll_result == granit::result::not_ready)
      break;
    if (poll_result.failed())
      return poll_result;
    if (input_event.window != window.ref())
      continue;
    if (ui != nullptr)
      ui->process(input_event);
    if (input != nullptr) {
      input->process(input_event, ui != nullptr && ui->wants_mouse(),
                     ui != nullptr && ui->wants_keyboard());
    }
  }
  if (result.ok() && output.resized)
    result = window.get_state(state);
  return result;
}

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
  const auto queue = summarize_profile(
      samples, [](const auto& sample) { return sample.render_queue_wait_ms; }, all);
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
  write_metric(json, "render_queue_wait", queue, true);
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

granit::result capture_loading_frame(const granit::window_state& window_state,
                                     const granit::swapchain_info& swapchain_info,
                                     granit::example::model_viewer::viewer_ui& ui,
                                     const char* stage, float progress,
                                     granit::example::imgui::frame_canvas_data& output) {
  ui.begin_frame(window_state, 1.0F / 60.0F);
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
  return ui.capture(output);
}

granit::result render_loading_frame(
    granit::example::model_viewer::desktop::threaded_render_service& service,
    const granit::window_state& window_state, const granit::swapchain_info& swapchain_info,
    granit::example::model_viewer::viewer_ui& ui, const char* stage, float progress) {
  granit::example::imgui::frame_canvas_data data;
  auto result = capture_loading_frame(window_state, swapchain_info, ui, stage, progress, data);
  if (result.ok())
    result = service.render_loading_frame(data);
  return result;
}

struct gpu_upload_progress_context {
  granit::example::model_viewer::desktop::threaded_render_service* service{};
  const std::array<granit::example::imgui::frame_canvas_data, 101>* progress_frames{};
};

struct cpu_asset_result {
  granit::result status{granit::result::unknown};
  std::vector<std::byte> environment_bytes;
  std::string diagnostic;
};

granit::result render_gpu_upload_progress(unsigned percentage, void* user_data) {
  auto& context = *static_cast<gpu_upload_progress_context*>(user_data);
  if (context.progress_frames == nullptr)
    return granit::result::success;
  return context.service->render_loading_frame((*context.progress_frames)[percentage]);
}

} // namespace

granit::example::model_viewer::desktop::application::application(options options)
    : options_(std::move(options)) {}

int granit::example::model_viewer::desktop::application::run() {
  using namespace granit::example::model_viewer;
  const auto& options = options_;
  auto result = granit::result::success;

  const auto initial_width = options.profile_output_path.empty() ? 1280 : 1920;
  const auto initial_height = options.profile_output_path.empty() ? 720 : 1080;
  granit::window_system window_system;
  if ((result = window_system.initialize({.backend = granit::window_backend::sdl3})).failed()) {
    std::cerr << "Window System 初始化失败：" << granit::result_message(result) << '\n';
    return 1;
  }
  granit::window window;
  result = window.initialize(window_system, {.title = "Granit Model Viewer",
                                             .width = static_cast<std::uint32_t>(initial_width),
                                             .height = static_cast<std::uint32_t>(initial_height),
                                             .flags = granit::window_flag::visible |
                                                      granit::window_flag::resizable |
                                                      granit::window_flag::high_dpi});
  if (result.failed()) {
    std::cerr << "Window 创建失败：" << granit::result_message(result) << '\n';
    return 1;
  }
  viewer_ui ui;
  if (options.show_ui)
    result = ui.initialize();
  viewer_session session;
  result = session.begin_renderer();
  granit::window_state window_state;
  if (result.ok())
    result = window.get_state(window_state);
  auto pixel_width = window_state.framebuffer_width;
  auto pixel_height = window_state.framebuffer_height;
  if (result.ok() && !options.profile_output_path.empty() &&
      (pixel_width != 1920 || pixel_height != 1080)) {
    std::cerr << "性能采样要求窗口像素尺寸为 1920x1080，实际为 " << pixel_width << 'x'
              << pixel_height << '\n';
    result = granit::result::invalid_argument;
  }
  desktop::threaded_render_service rendering;
  if (result.ok()) {
    result = rendering.initialize(window,
                                  {.application_name = "Granit Model Viewer",
                                   .enable_validation = options.enable_validation,
                                   .presentation = granit::presentation_mode::enabled,
                                   .backend = options.backend},
                                  {.width = static_cast<std::uint32_t>(pixel_width),
                                   .height = static_cast<std::uint32_t>(pixel_height),
                                   .presentation = options.presentation},
                                  session, options.show_ui);
  }
  granit::renderer_info renderer_info;
  granit::renderer_limits renderer_limits;
  granit::swapchain_info swapchain_info;
  if (result.ok()) {
    renderer_info = rendering.renderer_info();
    renderer_limits = rendering.renderer_limits();
    swapchain_info = rendering.swapchain_info();
  }
  render_quality_config render_quality{
      .sample_count = renderer_limits.supports_sample_count(granit::sample_count::four)
                          ? GRANIT_SAMPLE_COUNT_4
                          : GRANIT_SAMPLE_COUNT_1,
      .enable_fxaa = true,
      .enable_specular_aa = true,
      .sampler_anisotropy = renderer_limits.max_sampler_anisotropy >= 8.0F ? 8.0F : 1.0F};
  if (result.ok() && !options.profile_output_path.empty() &&
      swapchain_info.presentation != options.presentation) {
    std::cerr << "性能采样要求的呈现模式不可用，后端回退到了其他模式\n";
    result = granit::result::unsupported;
  }

  if (result.ok() && options.show_ui) {
    std::vector<std::byte> font_pixels;
    std::uint32_t font_width{};
    std::uint32_t font_height{};
    result = ui.capture_font_atlas(font_pixels, font_width, font_height);
    if (result.ok())
      result = rendering.initialize_font_atlas(font_pixels, font_width, font_height);
  }
  if (result.ok() && options.show_ui)
    result = ui.register_font(rendering.font_view(), rendering.font_sampler());

  granit::example::assets::asset_system assets;
  granit::example::assets::asset_mount model_mount;
  std::string model_path;
  if (result.ok() && (!assets.initialize(options.asset_path) ||
                      !assets.mount_location(options.asset_path, model_mount, model_path))) {
    result = granit::result::invalid_argument;
  }
  if (result.ok() && !session.start_loading(assets, {model_mount, model_path}))
    result = granit::result::internal;
  std::shared_ptr<granit::example::assets::asset_request> environment_request;
  if (result.ok() && !options.environment_path.empty()) {
    granit::example::assets::asset_mount environment_mount;
    std::string environment_path;
    if (!assets.mount_location(options.environment_path, environment_mount, environment_path)) {
      result = granit::result::invalid_argument;
    } else {
      environment_request = assets.request({environment_mount, environment_path});
    }
  }
  bool asset_bytes_ready = false;
  bool loading_cancelled = false;

  while (result.ok() && !asset_bytes_ready && !loading_cancelled) {
    session.poll_loading();
    desktop_window_events events;
    result = pump_window_events(window_system, window, window_state, events,
                                options.show_ui ? &ui : nullptr);
    loading_cancelled = events.close_requested;
    if (result.ok() && events.resized) {
      pixel_width = window_state.framebuffer_width;
      pixel_height = window_state.framebuffer_height;
      if (pixel_width > 0 && pixel_height > 0) {
        result = rendering.recreate_swapchain(
            {.width = pixel_width, .height = pixel_height, .presentation = options.presentation});
        if (result.ok())
          swapchain_info = rendering.swapchain_info();
      }
    }
    if (result.failed() || loading_cancelled)
      break;

    if (session.loading_status() == granit::example::model_viewer::model_loading_status::failed) {
      result = session.loading_result();
      break;
    }
    if (environment_request &&
        environment_request->status() == granit::example::assets::asset_request_status::failed) {
      session.fail(granit::result::invalid_argument,
                   std::string{environment_request->diagnostic()});
      result = granit::result::invalid_argument;
      break;
    }

    const bool environment_ready =
        !environment_request ||
        environment_request->status() == granit::example::assets::asset_request_status::ready;
    asset_bytes_ready = session.loading_status() ==
                            granit::example::model_viewer::model_loading_status::assets_ready &&
                        environment_ready;

    if (result.ok() && options.show_ui && !asset_bytes_ready) {
      const auto progress = session.loading_progress().document;
      const auto fraction = progress.total_bytes && *progress.total_bytes > 0
                                ? static_cast<float>(progress.received_bytes) /
                                      static_cast<float>(*progress.total_bytes)
                                : 0.0F;
      result = render_loading_frame(rendering, window_state, swapchain_info, ui,
                                    "Loading asset resources...",
                                    0.05F + std::min(fraction, 1.0F) * 0.20F);
      if (result == granit::result::out_of_date)
        result = granit::result::success;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{16});
  }

  if (loading_cancelled) {
    session.cancel_loading();
    if (environment_request)
      environment_request->cancel();
  }
  if (result.ok() && loading_cancelled)
    result = granit::result::not_ready;

  std::vector<std::byte> environment_bytes;
  std::atomic<unsigned> loading_stage{0};
  std::future<cpu_asset_result> cpu_loading;
  if (result.ok()) {
    cpu_loading = std::async(std::launch::async, [&] {
      cpu_asset_result output;
      loading_stage.store(3, std::memory_order_release);
      output.status = session.prepare_scene();
      if (output.status.failed()) {
        output.diagnostic = session.diagnostic();
        return output;
      }
      if (environment_request)
        output.environment_bytes = environment_request->bytes();
      loading_stage.store(4, std::memory_order_release);
      return output;
    });
  }
  while (result.ok() && cpu_loading.valid() &&
         cpu_loading.wait_for(std::chrono::milliseconds{0}) != std::future_status::ready) {
    desktop_window_events events;
    result = pump_window_events(window_system, window, window_state, events,
                                options.show_ui ? &ui : nullptr);
    loading_cancelled = events.close_requested;
    if (result.ok() && events.resized) {
      pixel_width = window_state.framebuffer_width;
      pixel_height = window_state.framebuffer_height;
      if (pixel_width > 0 && pixel_height > 0) {
        result = rendering.recreate_swapchain(
            {.width = pixel_width, .height = pixel_height, .presentation = options.presentation});
        if (result.ok())
          swapchain_info = rendering.swapchain_info();
      }
    }
    if (result.failed() || loading_cancelled || !options.show_ui)
      break;
    const auto stage = loading_stage.load(std::memory_order_acquire);
    const char* label =
        stage < 4 ? "Parsing glTF and decoding textures..." : "Planning GPU resources...";
    const auto progress = stage < 4 ? 0.30F : 0.38F;
    result = render_loading_frame(rendering, window_state, swapchain_info, ui, label, progress);
    if (result == granit::result::out_of_date)
      result = granit::result::success;
    std::this_thread::sleep_for(std::chrono::milliseconds{16});
  }
  if (loading_cancelled)
    session.cancel_loading();
  if (cpu_loading.valid()) {
    auto loaded = cpu_loading.get();
    if (result.ok() && loaded.status.failed()) {
      session.fail(loaded.status, std::move(loaded.diagnostic));
      result = loaded.status;
    }
    if (result.ok())
      environment_bytes = std::move(loaded.environment_bytes);
  }
  if (result.ok() && options.show_ui)
    result = render_loading_frame(rendering, window_state, swapchain_info, ui,
                                  "Preparing GPU upload...", 0.40F);
  std::array<granit::example::imgui::frame_canvas_data, 101> gpu_progress_frames;
  if (result.ok() && options.show_ui) {
    for (std::size_t percentage = 0; percentage < gpu_progress_frames.size(); ++percentage) {
      result = capture_loading_frame(window_state, swapchain_info, ui, "Uploading GPU resources...",
                                     static_cast<float>(percentage) / 100.0F,
                                     gpu_progress_frames[percentage]);
      if (result.failed())
        break;
    }
  }
  bool upload_resize_pending = false;
  if (result.ok()) {
    gpu_upload_progress_context upload_context{
        .service = &rendering, .progress_frames = options.show_ui ? &gpu_progress_frames : nullptr};
    result = rendering.begin_gpu_upload(
        {.environment_bytes = environment_bytes,
         .sampler_anisotropy = render_quality.sampler_anisotropy,
         .progress = options.show_ui ? render_gpu_upload_progress : nullptr,
         .progress_user_data = &upload_context});
    bool upload_completed = false;
    while (result.ok() && !upload_completed) {
      desktop_window_events events;
      result = pump_window_events(window_system, window, window_state, events,
                                  options.show_ui ? &ui : nullptr);
      if (events.close_requested)
        rendering.cancel_gpu_upload();
      if (events.resized) {
        pixel_width = window_state.framebuffer_width;
        pixel_height = window_state.framebuffer_height;
        upload_resize_pending = true;
      }
      upload_completed = rendering.try_finish_gpu_upload(result);
      std::this_thread::sleep_for(std::chrono::milliseconds{16});
    }
    if (!upload_completed) {
      const auto original_result = result;
      rendering.cancel_gpu_upload();
      const auto flush_result = rendering.flush();
      granit::result upload_result;
      upload_completed = rendering.try_finish_gpu_upload(upload_result);
      result = original_result.failed() ? original_result : flush_result;
      if (result.ok() && upload_completed)
        result = upload_result;
    }
    if (result.ok() && upload_resize_pending && pixel_width > 0 && pixel_height > 0) {
      result = rendering.recreate_swapchain({.width = static_cast<std::uint32_t>(pixel_width),
                                             .height = static_cast<std::uint32_t>(pixel_height),
                                             .presentation = options.presentation});
      if (result.ok())
        swapchain_info = rendering.swapchain_info();
    }
  }
  if (result.ok() && options.show_ui)
    result = render_loading_frame(rendering, window_state, swapchain_info, ui,
                                  "Creating render pipeline...", 0.96F);
  granit::render_pipeline_desc pipeline_desc{
      .samples = static_cast<granit::sample_count>(render_quality.sample_count),
      .enable_fxaa = render_quality.enable_fxaa != 0,
      .enable_specular_aa = render_quality.enable_specular_aa != 0};
  if (result.ok())
    result = rendering.initialize_pipeline(pipeline_desc);
  viewer_texture_previews previews;
  if (result.ok() && options.show_ui)
    result = previews.rebuild(session.cpu_scene(), session.scene_gpu(), ui);
  if (result.ok() && options.show_ui)
    result =
        render_loading_frame(rendering, window_state, swapchain_info, ui, "Loading complete", 1.0F);
  if (result.ok() && options.show_ui)
    result = rendering.finish_loading();

  if (result.failed()) {
    if (rendering.running()) {
      ui.clear_textures();
      static_cast<void>(rendering.shutdown());
    }
    std::cerr << "模型查看器初始化失败：" << granit::result_message(result);
    if (!session.diagnostic().empty())
      std::cerr << "（" << session.diagnostic() << "）";
    std::cerr << '\n';
    return 1;
  }
  const auto backend_name =
      renderer_info.backend == granit::renderer_backend::webgpu ? "WebGPU" : "Vulkan";
  viewer_input_accumulator input_adapter;
  bool running = true;
  bool recreate = false;
  bool recreate_surface = false;
  std::uint32_t rendered_frames = 0;
  std::vector<performance_sample> profile_samples;
  if (!options.profile_output_path.empty())
    profile_samples.reserve(1000);
  performance_sample latest_sample;
  bool has_pending_sample = false;
  std::unordered_map<std::uint64_t, float> producer_frame_times;
  auto last_ui_time = std::chrono::steady_clock::now();
  while (running) {
    const auto cpu_begin = std::chrono::steady_clock::now();
    frame_completion completed;
    while (rendering.try_take_completion(completed)) {
      const auto timing = producer_frame_times.find(completed.sequence);
      const auto producer_frame_ms = timing == producer_frame_times.end() ? 0.0F : timing->second;
      if (timing != producer_frame_times.end())
        producer_frame_times.erase(timing);
      if (completed.dropped)
        continue;
      const auto outcome =
          classify_presentation_result(completed.status, completed.execution.needs_recreate);
      if (outcome.action == presentation_action::recreate_swapchain)
        recreate = true;
      else if (outcome.action == presentation_action::recreate_surface)
        recreate_surface = true;
      else if (outcome.action == presentation_action::stop) {
        result = completed.status;
        running = false;
        break;
      }
      if (!outcome.frame_rendered)
        continue;
      latest_sample = {.frames_per_second =
                           producer_frame_ms > 0.0F ? 1000.0F / producer_frame_ms : 0.0F,
                       .cpu_frame_ms = producer_frame_ms,
                       .render_queue_wait_ms = completed.execution.queue_wait_ms,
                       .frame_slot_wait_ms = completed.execution.acquire_wait_ms,
                       .present_wait_ms = completed.execution.present_wait_ms,
                       .gpu_frame_ms = completed.execution.gpu_frame_ms,
                       .gpu_timing_available = completed.execution.gpu_timing_available};
      has_pending_sample = true;
      if (!options.profile_output_path.empty() && rendered_frames >= 300)
        profile_samples.push_back(latest_sample);
      ++rendered_frames;
    }
    if (!running)
      break;
    input_adapter.begin_frame();
    desktop_window_events events;
    result = pump_window_events(window_system, window, window_state, events,
                                options.show_ui ? &ui : nullptr, &input_adapter);
    if (result.failed())
      break;
    running = !events.close_requested;
    if (events.resized) {
      pixel_width = window_state.framebuffer_width;
      pixel_height = window_state.framebuffer_height;
      recreate = true;
    }
    if (!running)
      break;
    if (pixel_width <= 0 || pixel_height <= 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds{16});
      continue;
    }
    if (recreate_surface) {
      result = rendering.flush();
      if (result.failed())
        break;
      result =
          rendering.recreate_surface(window, {.width = static_cast<std::uint32_t>(pixel_width),
                                              .height = static_cast<std::uint32_t>(pixel_height),
                                              .presentation = options.presentation});
      if (result.failed()) {
        break;
      }
      swapchain_info = rendering.swapchain_info();
      recreate_surface = false;
      recreate = false;
    }
    if (recreate) {
      result = rendering.recreate_swapchain({.width = static_cast<std::uint32_t>(pixel_width),
                                             .height = static_cast<std::uint32_t>(pixel_height),
                                             .presentation = options.presentation});
      if (result == granit::result::not_ready)
        continue;
      if (result.failed())
        break;
      swapchain_info = rendering.swapchain_info();
      recreate = false;
    }

    if (!rendering.can_submit_frame()) {
      rendering.record_skipped_frame_build();
      continue;
    }

    const auto ui_time = std::chrono::steady_clock::now();
    const auto delta_seconds = std::chrono::duration<float>(ui_time - last_ui_time).count();
    last_ui_time = ui_time;
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
    const auto queue_stats = rendering.query_queue_stats();
    const performance_panel_info panel_performance{
        .frames_per_second = latest_sample.frames_per_second,
        .cpu_frame_ms = latest_sample.cpu_frame_ms,
        .render_queue_wait_ms = latest_sample.render_queue_wait_ms,
        .frame_slot_wait_ms = latest_sample.frame_slot_wait_ms,
        .present_wait_ms = latest_sample.present_wait_ms,
        .gpu_frame_ms = latest_sample.gpu_frame_ms,
        .gpu_timing_available = latest_sample.gpu_timing_available,
        .queue_high_watermark = queue_stats.pending_high_watermark,
        .replaced_frames = queue_stats.replaced_frames,
        .skipped_frame_builds = queue_stats.skipped_frame_builds,
        .merged_input_frames = input_adapter.merged_input_frames(),
        .render_lag_ms = queue_stats.render_lag_ms,
        .history = session.performance().summarize()};
    viewer_frame_build_result frame;
    result = build_viewer_frame(session, ui, input_adapter,
                                {.window = window_state,
                                 .delta_seconds = delta_seconds,
                                 .renderer = panel_renderer,
                                 .performance = panel_performance,
                                 .quality = render_quality,
                                 .previews = previews.items(),
                                 .sample = has_pending_sample
                                               ? std::optional<performance_sample>{latest_sample}
                                               : std::nullopt,
                                 .show_ui = options.show_ui},
                                frame);
    if (result.failed())
      break;

    if (frame.changes.quality) {
      granit::render_pipeline_desc replacement_desc{
          .samples = static_cast<granit::sample_count>(frame.changes.quality->sample_count),
          .enable_fxaa = frame.changes.quality->enable_fxaa != 0,
          .enable_specular_aa = frame.changes.quality->enable_specular_aa != 0};
      desktop::quality_change_result quality_result;
      result = rendering.change_quality(replacement_desc, frame.changes.quality->sampler_anisotropy,
                                        frame.changes.quality->sampler_anisotropy !=
                                            render_quality.sampler_anisotropy,
                                        quality_result);
      if (result.ok() && quality_result.scene_reuploaded) {
        if (result.ok() && options.show_ui)
          result = previews.rebuild(session.cpu_scene(), session.scene_gpu(), ui);
        frame.packet.canvas.clear();
      }
      if (result.ok()) {
        render_quality = *frame.changes.quality;
      }
    }
    if (result.failed())
      break;

    if (result.ok()) {
      if (frame.changes.material &&
          session.state().selected_material() != granit::example::gltf::invalid_index) {
        result =
            rendering.update_material(session.state().selected_material(), *frame.changes.material);
      }
    }
    if (result.failed())
      break;
    [[maybe_unused]] std::uint64_t submitted_sequence{};
    result = rendering.submit(std::move(frame.packet), submitted_sequence);
    const auto producer_frame_ms =
        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - cpu_begin)
            .count();
    if (result.ok())
      producer_frame_times.emplace(submitted_sequence, producer_frame_ms);
    if (!options.profile_output_path.empty() && profile_samples.size() >= 1000)
      break;
    if (options.profile_output_path.empty() && options.smoke_test && rendered_frames >= 3)
      break;
  }

  if (rendering.running()) {
    ui.clear_textures();
    const auto shutdown_result = rendering.shutdown();
    if (result.ok())
      result = shutdown_result;
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
