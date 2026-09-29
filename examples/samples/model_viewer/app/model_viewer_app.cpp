// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/app/model_viewer_app.h"

#include "assets/asset_manager.h"
#include "camera/orbit_camera_input_accumulator.h"
#include "model_viewer/app/model_load_operation.h"
#include "model_viewer/model/viewer_document.h"
#include "model_viewer/rendering/frame_builder.h"
#include "model_viewer/rendering/presentation_recovery.h"
#include "model_viewer/rendering/render_dispatcher.h"
#include "model_viewer/rendering/render_execution.h"
#include "model_viewer/ui/texture_previews.h"
#include "model_viewer/ui/viewer_ui.h"

#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <new>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace granit::example::model_viewer {
namespace {

enum class viewer_runtime_phase {
  starting,
  renderer_wait,
  asset_wait,
  scene_prepare,
  gpu_upload,
  pipeline_prepare,
  finalize,
  ready,
  failed,
  stopped,
};

constexpr std::string_view backend_name(granit::renderer_backend backend) noexcept {
  return backend == granit::renderer_backend::webgpu ? "WebGPU" : "Vulkan";
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

struct profile_metric {
  float p50{};
  float p95{};
  float p99{};
  std::size_t count{};
};

template <typename Selector, typename Filter>
profile_metric summarize_profile(std::span<const performance_sample> samples, Selector selector,
                                 Filter filter) {
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
    return values[((values.size() - 1) * numerator + 99) / 100];
  };
  return {
      .p50 = percentile(50), .p95 = percentile(95), .p99 = percentile(99), .count = values.size()};
}

bool write_profile(const std::filesystem::path& path, const granit::renderer_info& renderer,
                   const granit::swapchain_info& swapchain, std::string_view asset, bool validation,
                   bool ui, std::span<const performance_sample> samples) {
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
       << "  \"asset\": " << std::quoted(std::string{asset}) << ",\n"
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
  std::error_code error;
  if (path.has_parent_path())
    std::filesystem::create_directories(path.parent_path(), error);
  if (error)
    return false;
  std::ofstream output(path);
  output << json.str();
  return static_cast<bool>(output);
}

granit::result capture_loading_frame(const granit::window_state& window_state,
                                     const granit::swapchain_info& swapchain_info, viewer_ui& ui,
                                     const char* stage, float progress,
                                     imgui::frame_canvas_data& output) {
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
  ImGui::TextDisabled("The window remains responsive while assets are prepared.");
  ImGui::End();
  return ui.capture(output);
}

const char* upload_stage_label(gltf_rendering::scene_upload_stage stage) noexcept {
  using enum gltf_rendering::scene_upload_stage;
  switch (stage) {
  case planning:
    return "Planning GPU resources...";
  case geometry:
    return "Uploading geometry...";
  case textures:
    return "Uploading textures...";
  case samplers:
    return "Creating samplers...";
  case meshes:
    return "Creating meshes...";
  case materials:
    return "Creating materials...";
  }
  return "Uploading scene...";
}

float upload_stage_fraction(const gltf_rendering::scene_upload_progress& progress) noexcept {
  constexpr float begin = 0.40F;
  constexpr float extent = 0.45F;
  constexpr float stage_count = 6.0F;
  const auto stage = static_cast<float>(progress.stage);
  const auto local =
      progress.total == 0
          ? 0.0F
          : std::clamp(static_cast<float>(progress.completed) / static_cast<float>(progress.total),
                       0.0F, 1.0F);
  return begin + extent * (stage + local) / stage_count;
}

} // namespace

struct model_viewer_app::implementation {
  model_viewer_app_desc desc;
  viewer_document document;
  model_load_operation model_load;
  gltf_rendering::scene_plan scene_plan;
  std::unique_ptr<render_execution_policy> executor;
  render_dispatcher rendering;
  viewer_ui ui;
  camera::orbit_camera_input_accumulator input;
  texture_previews previews;
  viewer_runtime_phase phase{viewer_runtime_phase::starting};
  granit::result terminal_result{granit::result::success};
  std::string failure_stage;
  assets::asset_handle<assets::asset_blob> environment_request;
  std::vector<std::byte> environment_bytes;
  render_quality_config quality;
  granit::window_state window_state;
  bool recreate_swapchain{};
  bool recreate_surface{};
  bool renderer_observed{};
  bool presentation_observed{};
  bool upload_started{};
  bool pipeline_prepare_started{};
  bool pipeline_observer_complete{};
  bool shutdown_complete{};
  std::atomic<bool> cancel_requested{};
  std::uint64_t upload_sequence{};
  mutable std::mutex upload_progress_mutex;
  gltf_rendering::scene_upload_progress upload_progress{};
  granit::result loading_render_result{granit::result::success};
  performance_sample latest_performance{};
  bool has_performance{};
  std::unordered_map<std::uint64_t, float> submitted_frames;
  std::chrono::steady_clock::time_point renderer_started{};
  std::uint64_t shutdown_live_resource_count{};
  std::uint64_t shutdown_pending_retirement_count{};
  granit::result shutdown_result{granit::result::success};
  unsigned input_event_count{};
  unsigned loading_frame_count{};
  unsigned rendered_frame_count{};
  unsigned applied_input_count{};
  unsigned resize_count{};
  unsigned quality_generation{};
  unsigned lighting_generation{};
  bool asset_ready{};
  std::vector<performance_sample> profile_samples;

  void notify(model_viewer_app_status status, std::string_view stage,
              granit::result result) noexcept {
    if (desc.observer != nullptr)
      desc.observer->on_status(status, stage, result);
  }

  void fail(model_viewer_app& owner, std::string_view stage, granit::result result) noexcept {
    if (phase == viewer_runtime_phase::failed || phase == viewer_runtime_phase::stopped)
      return;
    terminal_result = result.failed() ? result : granit::result::internal;
    failure_stage.assign(stage);
    phase = viewer_runtime_phase::failed;
    notify(model_viewer_app_status::failed, failure_stage, terminal_result);
    if (!desc.keep_alive_on_failure)
      owner.request_shutdown();
  }

  [[nodiscard]] std::string_view failure_stage_name() const noexcept {
    switch (phase) {
    case viewer_runtime_phase::renderer_wait:
      return "provider-events";
    case viewer_runtime_phase::asset_wait:
      return model_load.error() == model_load_error::resource_read ? "asset-resource-fetch"
                                                                   : "asset-fetch";
    case viewer_runtime_phase::scene_prepare:
      return "asset-load";
    case viewer_runtime_phase::gpu_upload:
      return "asset-upload";
    case viewer_runtime_phase::pipeline_prepare:
      return "renderer-pipeline";
    case viewer_runtime_phase::finalize:
      return "pipeline-create";
    case viewer_runtime_phase::ready:
      return "model-viewer-frame";
    default:
      return "update";
    }
  }

  static void diagnose(granit_diagnostic_severity severity, granit_diagnostic_category,
                       const char* message, std::uint32_t length, void* user_data) noexcept {
    auto& self = *static_cast<implementation*>(user_data);
    if (self.desc.observer != nullptr) {
      self.desc.observer->on_diagnostic(static_cast<granit::diagnostic_severity>(severity),
                                        std::string_view{message, length});
    }
  }

  static bool scene_progress(const gltf::import_progress& progress, void* user_data) {
    auto& self = *static_cast<implementation*>(user_data);
    if (self.desc.execution == viewer_execution_mode::inline_current_thread) {
      self.loading_render_result =
          self.render_loading("Parsing glTF and decoding textures...", 0.30F);
      if (self.loading_render_result.failed())
        return false;
    }
    return !self.cancel_requested.load(std::memory_order_acquire) &&
           (self.desc.observer == nullptr ||
            self.desc.observer->on_scene_prepare_progress(progress));
  }

  static bool upload_progress_callback(const gltf_rendering::scene_upload_progress& progress,
                                       void* user_data) {
    auto& self = *static_cast<implementation*>(user_data);
    {
      const std::scoped_lock lock{self.upload_progress_mutex};
      self.upload_progress = progress;
    }
    if (self.desc.execution == viewer_execution_mode::inline_current_thread) {
      self.loading_render_result =
          self.render_loading(upload_stage_label(progress.stage), upload_stage_fraction(progress));
      if (self.loading_render_result.failed())
        return false;
    }
    return !self.cancel_requested.load(std::memory_order_acquire) &&
           (self.desc.observer == nullptr || self.desc.observer->on_gpu_upload_progress(progress));
  }

  [[nodiscard]] gltf_rendering::scene_upload_progress upload_progress_snapshot() const noexcept {
    const std::scoped_lock lock{upload_progress_mutex};
    return upload_progress;
  }

  granit::result render_loading(const char* label, float progress) {
    if (!desc.show_ui || !rendering.presentation_valid())
      return granit::result::success;
    imgui::frame_canvas_data frame;
    auto result =
        capture_loading_frame(window_state, rendering.swapchain_info(), ui, label, progress, frame);
    if (result.ok())
      result = rendering.render_loading_frame(frame);
    if (result.ok())
      ++loading_frame_count;
    return result == granit::result::out_of_date ? granit::result::success : result;
  }

  granit::result recover_presentation(model_viewer_app& owner) {
    auto result = owner.app_window().get_state(window_state);
    if (result.failed())
      return result;
    if (window_state.framebuffer_width == 0 || window_state.framebuffer_height == 0)
      return granit::result::not_ready;
    const auto& current = rendering.swapchain_info();
    const bool size_changed = current.width != window_state.framebuffer_width ||
                              current.height != window_state.framebuffer_height;
    if (!recreate_surface && !recreate_swapchain && !size_changed)
      return granit::result::success;
    const granit::swapchain_desc swapchain_desc{
        .width = window_state.framebuffer_width,
        .height = window_state.framebuffer_height,
        .minimum_image_count = 2,
        .presentation = desc.present_mode,
    };
    result = recreate_surface ? rendering.recreate_surface(owner.app_window(), swapchain_desc)
                              : rendering.recreate_swapchain(swapchain_desc);
    if (result.ok()) {
      recreate_surface = false;
      recreate_swapchain = false;
      ++resize_count;
    }
    return result;
  }

  granit::result initialize_ready_renderer(model_viewer_app& owner) {
    auto result = rendering.complete_renderer_initialization();
    if (result.failed())
      return result;

    const auto& limits = rendering.renderer_limits();
    if (limits.uniform_buffer_offset_alignment == 0 ||
        limits.max_uniform_buffer_binding_size == 0) {
      return granit::result::internal;
    }
    if (!limits.supports_sample_count(quality.sample_count))
      quality.sample_count = granit::sample_count::one;
    quality.sampler_anisotropy =
        std::clamp(quality.sampler_anisotropy, 1.0F, limits.max_sampler_anisotropy);
    if (desc.observer != nullptr && !renderer_observed) {
      result = desc.observer->on_renderer_ready(rendering);
      if (result.failed())
        return result;
      renderer_observed = true;
    }
    result = owner.app_window().get_state(window_state);
    if (result.ok() && !desc.profile_output_path.empty() &&
        (window_state.framebuffer_width != 1920 || window_state.framebuffer_height != 1080)) {
      result = granit::result::invalid_argument;
    }
    if (result.ok()) {
      result = rendering.initialize_presentation(owner.app_window(),
                                                 {.width = window_state.framebuffer_width,
                                                  .height = window_state.framebuffer_height,
                                                  .minimum_image_count = 2,
                                                  .presentation = desc.present_mode},
                                                 desc.show_ui);
    }
    if (result.ok() && desc.observer != nullptr && !presentation_observed) {
      result = desc.observer->on_presentation_ready(rendering);
      if (result.ok())
        presentation_observed = true;
    }
    if (result.ok() && !desc.profile_output_path.empty() &&
        rendering.swapchain_info().presentation != desc.present_mode) {
      result = granit::result::unsupported;
    }
    if (result.ok() && desc.show_ui) {
      std::vector<std::byte> font_pixels;
      std::uint32_t width{};
      std::uint32_t height{};
      result = ui.capture_font_atlas(font_pixels, width, height);
      if (result.ok())
        result = rendering.initialize_font_atlas(font_pixels, width, height);
      if (result.ok())
        result = ui.register_font(rendering.font_view(), rendering.font_sampler());
    }
    return result;
  }

  granit::result update_renderer_wait(model_viewer_app& owner) {
    auto result = rendering.process_renderer_events();
    granit::renderer_status renderer_status;
    if (result.ok())
      result = rendering.query_renderer_status(renderer_status);
    if (result.failed())
      return result;
    if (renderer_status.state == granit::renderer_state::failed ||
        renderer_status.state == granit::renderer_state::device_lost) {
      return renderer_status.failure_result.failed() ? renderer_status.failure_result
                                                     : granit::result::initialization_failed;
    }
    if (renderer_status.state != granit::renderer_state::ready) {
      constexpr auto timeout = std::chrono::seconds{30};
      return std::chrono::steady_clock::now() - renderer_started >= timeout
                 ? granit::result::not_ready
                 : granit::result::success;
    }
    result = initialize_ready_renderer(owner);
    if (result.ok())
      phase = viewer_runtime_phase::asset_wait;
    return result;
  }

  granit::result update_asset_wait() {
    model_load.poll_assets();
    if (model_load.status() == model_load_status::failed)
      return model_load.result();
    if (environment_request && environment_request.status() == assets::asset_status::failed)
      return granit::result::invalid_argument;
    const bool environment_ready = !environment_request || environment_request.ready();
    if (model_load.status() == model_load_status::assets_ready && environment_ready) {
      if (environment_request)
        environment_bytes = environment_request.value()->bytes;
      phase = viewer_runtime_phase::scene_prepare;
      return granit::result::success;
    }
    const auto progress = model_load.progress();
    const auto fraction = progress.total_bytes && *progress.total_bytes > 0
                              ? static_cast<float>(progress.completed_bytes) /
                                    static_cast<float>(*progress.total_bytes)
                              : 0.0F;
    return render_loading("Loading asset resources...", 0.05F + std::min(fraction, 1.0F) * 0.20F);
  }

  granit::result update_scene_prepare() {
    if (model_load.status() == model_load_status::assets_ready) {
      const auto loading_result = render_loading("Parsing glTF and decoding textures...", 0.30F);
      if (loading_result.failed())
        return loading_result;
      cancel_requested.store(false, std::memory_order_release);
      const auto begin_result = model_load.begin_prepare(scene_progress, this);
      if (begin_result.failed())
        return begin_result;
    }
    auto result = model_load.poll_prepare();
    if (result == granit::result::not_ready) {
      const auto loading_result = render_loading("Parsing glTF and decoding textures...", 0.30F);
      return loading_result.failed() ? loading_result : granit::result::success;
    }
    if (result.ok()) {
      gltf::scene scene;
      gltf_rendering::scene_plan plan;
      if (!model_load.take(scene, plan))
        return granit::result::internal;
      try {
        document.assign(std::move(scene));
        scene_plan = std::move(plan);
      } catch (const std::bad_alloc&) {
        return granit::result::out_of_memory;
      }
      phase = viewer_runtime_phase::gpu_upload;
    }
    return result;
  }

  granit::result update_gpu_upload() {
    if (!upload_started) {
      const auto loading_result = render_loading("Planning GPU resources...", 0.40F);
      if (loading_result.failed())
        return loading_result;
      cancel_requested.store(false, std::memory_order_release);
      gltf::scene scene;
      gltf_rendering::scene_plan plan;
      granit::result result;
      try {
        scene = document.scene();
        plan = scene_plan;
        result = granit::result::success;
      } catch (const std::bad_alloc&) {
        result = granit::result::out_of_memory;
      }
      if (result.ok()) {
        result = rendering.begin_upload_scene(std::move(scene), std::move(plan), environment_bytes,
                                              quality.sampler_anisotropy, upload_progress_callback,
                                              this, upload_sequence);
      }
      if (result.failed())
        return result;
      upload_started = true;
    }
    render_task_completion completion;
    if (!rendering.try_take_control_completion(completion)) {
      if (desc.execution == viewer_execution_mode::dedicated_thread) {
        const auto progress = upload_progress_snapshot();
        return render_loading(upload_stage_label(progress.stage), upload_stage_fraction(progress));
      }
      return loading_render_result;
    }
    if (completion.sequence != upload_sequence)
      return granit::result::internal;
    if (loading_render_result.failed())
      return loading_render_result;
    if (completion.status.failed())
      return completion.status;
    const auto& environment = rendering.environment_info();
    const auto exposure = environment_bytes.empty() ? document.state().exposure_ev()
                                                    : environment.recommended_exposure_ev;
    const auto intensity = environment_bytes.empty() ? document.state().environment_intensity()
                                                     : environment.environment.intensity;
    viewer_change recommended_lighting;
    recommended_lighting.exposure_ev = exposure;
    recommended_lighting.environment_intensity = intensity;
    const auto complete_result = document.apply(recommended_lighting) == viewer_state_error::none
                                     ? granit::result::success
                                     : granit::result::invalid_argument;
    if (complete_result.failed())
      return complete_result;
    asset_ready = true;
    phase = viewer_runtime_phase::pipeline_prepare;
    return granit::result::success;
  }

  granit::result update_pipeline_prepare() {
    auto result = rendering.process_renderer_events();
    if (result.failed())
      return result;
    if (!pipeline_prepare_started) {
      result =
          rendering.begin_pipeline_prepare(rendering.swapchain_info().format, quality.sample_count);
      if (result.failed())
        return result;
      pipeline_prepare_started = true;
    }
    result = rendering.poll_pipeline_prepare();
    if (result == granit::result::not_ready)
      return render_loading("Preparing rendering pipelines...", 0.90F);
    if (result.failed())
      return result;
    if (desc.observer != nullptr && !pipeline_observer_complete) {
      result = desc.observer->on_pipeline_ready(rendering);
      if (result == granit::result::not_ready)
        return render_loading("Validating rendering pipelines...", 0.95F);
      if (result.failed())
        return result;
      pipeline_observer_complete = true;
    }
    phase = viewer_runtime_phase::finalize;
    return granit::result::success;
  }

  granit::result update_finalize() {
    const granit::render_pipeline_desc pipeline_desc{
        .samples = quality.sample_count,
        .enable_fxaa = quality.enable_fxaa,
        .enable_specular_aa = quality.enable_specular_aa,
    };
    auto result = rendering.initialize_pipeline(pipeline_desc);
    if (result.ok() && desc.show_ui)
      result = previews.rebuild(document.scene(), rendering, ui);
    if (result.ok() && desc.show_ui)
      result = render_loading("Loading complete", 1.0F);
    if (result.ok() && desc.show_ui)
      result = rendering.finish_loading();
    if (result.ok()) {
      phase = viewer_runtime_phase::ready;
      notify(model_viewer_app_status::ready, "ready", granit::result::success);
    }
    return result;
  }

  void consume_frame_completions(model_viewer_app& owner) {
    frame_completion completed;
    while (rendering.try_take_frame_completion(completed)) {
      const auto submitted = submitted_frames.find(completed.sequence);
      float producer_ms{};
      if (submitted != submitted_frames.end()) {
        producer_ms = submitted->second;
        submitted_frames.erase(submitted);
      }
      if (completed.dropped)
        continue;
      const auto outcome =
          classify_presentation_result(completed.status, completed.execution.needs_recreate);
      if (outcome.action == presentation_action::recreate_surface)
        recreate_surface = true;
      else if (outcome.action == presentation_action::recreate_swapchain)
        recreate_swapchain = true;
      else if (outcome.action == presentation_action::stop) {
        fail(owner, "frame-submit", completed.status);
        return;
      }
      if (!outcome.frame_rendered)
        continue;
      latest_performance = {
          .frames_per_second = producer_ms > 0.0F ? 1000.0F / producer_ms : 0.0F,
          .cpu_frame_ms = producer_ms,
          .render_queue_wait_ms = completed.execution.queue_wait_ms,
          .frame_slot_wait_ms = completed.execution.acquire_wait_ms,
          .present_wait_ms = completed.execution.present_wait_ms,
          .gpu_frame_ms = completed.execution.gpu_frame_ms,
          .gpu_timing_available = completed.execution.gpu_timing_available,
      };
      has_performance = true;
      if (!desc.profile_output_path.empty() && rendered_frame_count >= 300)
        profile_samples.push_back(latest_performance);
      ++rendered_frame_count;
    }
  }

  granit::result update_ready(model_viewer_app& owner, float delta_seconds) {
    const auto cpu_begin = std::chrono::steady_clock::now();
    consume_frame_completions(owner);
    if (phase == viewer_runtime_phase::failed)
      return granit::result::success;
    if (desc.smoke_test && rendered_frame_count >= 3) {
      owner.request_shutdown();
      return granit::result::success;
    }
    if (!desc.profile_output_path.empty() && profile_samples.size() >= 1000) {
      owner.request_shutdown();
      return granit::result::success;
    }
    auto result = recover_presentation(owner);
    if (result == granit::result::not_ready)
      return granit::result::success;
    if (result.failed())
      return result;
    if (!rendering.can_submit_frame()) {
      rendering.record_skipped_frame_build();
      return granit::result::success;
    }
    const auto& info = rendering.renderer_info();
    const auto& limits = rendering.renderer_limits();
    const auto& swapchain = rendering.swapchain_info();
    const auto queue = rendering.query_queue_stats();
    viewer_frame_build_result frame;
    result = build_viewer_frame(
        document, scene_plan, ui, input,
        {.window = window_state,
         .delta_seconds = delta_seconds,
         .renderer = {.backend = backend_name(info.backend),
                      .adapter = info.adapter_name,
                      .swapchain_format = "Swapchain",
                      .present_mode = present_mode_label(swapchain.presentation),
                      .width = swapchain.width,
                      .height = swapchain.height,
                      .frame_slots = GRANIT_DEFAULT_FRAMES_IN_FLIGHT,
                      .supported_sample_counts = limits.framebuffer_sample_counts,
                      .max_sampler_anisotropy = limits.max_sampler_anisotropy},
         .performance = {.frames_per_second = latest_performance.frames_per_second,
                         .cpu_frame_ms = latest_performance.cpu_frame_ms,
                         .render_queue_wait_ms = latest_performance.render_queue_wait_ms,
                         .frame_slot_wait_ms = latest_performance.frame_slot_wait_ms,
                         .present_wait_ms = latest_performance.present_wait_ms,
                         .gpu_frame_ms = latest_performance.gpu_frame_ms,
                         .gpu_timing_available = latest_performance.gpu_timing_available,
                         .queue_high_watermark = queue.pending_high_watermark,
                         .replaced_frames = queue.replaced_frames,
                         .skipped_frame_builds = queue.skipped_frame_builds,
                         .merged_input_frames = input.merged_input_frames(),
                         .render_lag_ms = queue.render_lag_ms,
                         .history = document.performance().summarize()},
         .quality = quality,
         .previews = previews.items(),
         .sample =
             has_performance ? std::optional<performance_sample>{latest_performance} : std::nullopt,
         .show_ui = desc.show_ui},
        frame);
    if (result.failed())
      return result;
    if (frame.input_applied)
      ++applied_input_count;
    input.begin_frame();
    if (frame.changes.state.debug_display) {
      result = rendering.update_debug_display(
          static_cast<std::uint32_t>(*frame.changes.state.debug_display));
    }
    if (result.ok() && frame.changes.quality) {
      const bool reupload = frame.changes.quality->sampler_anisotropy != quality.sampler_anisotropy;
      result = configure_quality(*frame.changes.quality);
      if (result.ok() && reupload && desc.show_ui) {
        result = previews.rebuild(document.scene(), rendering, ui);
        frame.packet.canvas.clear();
      }
    }
    if (result.ok() && frame.changes.material &&
        document.state().selected_material() != gltf::invalid_index) {
      result =
          rendering.update_material(document.state().selected_material(), *frame.changes.material);
      if (result.ok()) {
        result =
            document.update_material(document.state().selected_material(), *frame.changes.material);
      }
    }
    if (result.failed())
      return result;
    std::uint64_t sequence{};
    const auto submitted_at = std::chrono::steady_clock::now();
    result = rendering.submit_frame(std::move(frame.packet), sequence);
    if (result.ok()) {
      const auto producer_ms =
          std::chrono::duration<float, std::milli>(submitted_at - cpu_begin).count();
      submitted_frames.emplace(sequence, producer_ms);
    }
    return result;
  }

  granit::result configure_quality(const render_quality_config& replacement) {
    if (phase != viewer_runtime_phase::ready || !rendering.valid())
      return granit::result::not_ready;
    const auto& limits = rendering.renderer_limits();
    if ((replacement.sample_count != granit::sample_count::one &&
         replacement.sample_count != granit::sample_count::four) ||
        replacement.sampler_anisotropy < 1.0F) {
      return granit::result::invalid_argument;
    }
    if (!limits.supports_sample_count(replacement.sample_count) ||
        replacement.sampler_anisotropy > limits.max_sampler_anisotropy) {
      return granit::result::unsupported;
    }
    const granit::render_pipeline_desc pipeline_desc{
        .samples = replacement.sample_count,
        .enable_fxaa = replacement.enable_fxaa,
        .enable_specular_aa = replacement.enable_specular_aa,
    };
    render_quality_change_result output;
    const auto result = rendering.change_quality(
        pipeline_desc, replacement.sampler_anisotropy,
        replacement.sampler_anisotropy != quality.sampler_anisotropy, output);
    if (result.ok()) {
      quality = replacement;
      ++quality_generation;
    }
    return result;
  }
};

model_viewer_app::model_viewer_app() : state_(std::make_unique<implementation>()) {}

model_viewer_app::~model_viewer_app() = default;

granit::result model_viewer_app::run(const model_viewer_app_desc& desc) noexcept {
  if (state_->phase != viewer_runtime_phase::starting || desc.model_location.empty())
    return granit::result::invalid_argument;
  try {
    state_->desc = desc;
    state_->quality = desc.initial_quality;
    if (!desc.profile_output_path.empty())
      state_->profile_samples.reserve(1000);
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  }
  const auto result = run_host(state_->desc.host);
  return result.failed() ? result : state_->terminal_result;
}

granit::result model_viewer_app::on_host_initialize() noexcept {
  auto& state = *state_;
  auto result = state.desc.show_ui ? state.ui.initialize() : granit::result::success;
  try {
    if (result.ok()) {
      if (state.desc.execution == viewer_execution_mode::dedicated_thread)
        state.executor = std::make_unique<threaded_render_execution>();
      else
        state.executor = std::make_unique<inline_render_execution>();
    }
  } catch (const std::bad_alloc&) {
    result = granit::result::out_of_memory;
  }
  if (result.ok()) {
    result = state.rendering.initialize_renderer(
        *state.executor, {.application_name = "Granit Model Viewer",
                          .enable_validation = state.desc.enable_validation,
                          .presentation = granit::presentation_mode::enabled,
                          .diagnostics = implementation::diagnose,
                          .diagnostic_user_data = &state,
                          .backend = state.desc.renderer_backend});
  }
  if (result.ok() && !state.model_load.start(
                         assets(), assets::asset_location::external(state.desc.model_location))) {
    result = granit::result::invalid_argument;
  }
  if (result.ok() && !state.desc.environment_location.empty()) {
    state.environment_request = assets().load<assets::asset_blob>(
        assets::asset_location::external(state.desc.environment_location));
    if (!state.environment_request.valid())
      result = granit::result::out_of_memory;
  }
  if (result.failed()) {
    state.fail(*this, "initialize", result);
    return result;
  }
  state.renderer_started = std::chrono::steady_clock::now();
  state.phase = viewer_runtime_phase::renderer_wait;
  state.notify(model_viewer_app_status::loading, "renderer", granit::result::success);
  return granit::result::success;
}

granit::result model_viewer_app::on_host_update(float delta_seconds,
                                                granit::window_loop_action& action) noexcept {
  auto& state = *state_;
  if (state.phase == viewer_runtime_phase::failed) {
    action = granit::window_loop_action::idle;
    return granit::result::success;
  }
  granit::result result;
  try {
    switch (state.phase) {
    case viewer_runtime_phase::renderer_wait:
      result = state.update_renderer_wait(*this);
      break;
    case viewer_runtime_phase::asset_wait:
      result = state.update_asset_wait();
      break;
    case viewer_runtime_phase::scene_prepare:
      result = state.update_scene_prepare();
      break;
    case viewer_runtime_phase::gpu_upload:
      result = state.update_gpu_upload();
      break;
    case viewer_runtime_phase::pipeline_prepare:
      result = state.update_pipeline_prepare();
      break;
    case viewer_runtime_phase::finalize:
      result = state.update_finalize();
      break;
    case viewer_runtime_phase::ready:
      result = state.update_ready(*this, delta_seconds);
      break;
    default:
      result = granit::result::success;
      break;
    }
  } catch (const std::bad_alloc&) {
    result = granit::result::out_of_memory;
  } catch (...) {
    result = granit::result::internal;
  }
  if (result.failed())
    state.fail(*this, state.failure_stage_name(), result);
  if (state.phase == viewer_runtime_phase::failed)
    action = granit::window_loop_action::idle;
  return granit::result::success;
}

granit::result model_viewer_app::on_host_window_event(const granit::window_event& event) noexcept {
  auto& state = *state_;
  if (event.type == granit::window_event_type::focus_changed)
    ++state.input_event_count;
  if (event.type == granit::window_event_type::resized ||
      event.type == granit::window_event_type::scale_changed) {
    state.recreate_swapchain = true;
  }
  if (state.desc.show_ui)
    state.ui.process(event);
  state.input.process(event);
  return granit::result::success;
}

granit::result model_viewer_app::on_host_input_event(const granit::input_event& event) noexcept {
  auto& state = *state_;
  ++state.input_event_count;
  if (state.desc.show_ui)
    state.ui.process(event);
  state.input.process(event, state.desc.show_ui && state.ui.wants_mouse(),
                      state.desc.show_ui && state.ui.wants_keyboard());
  return granit::result::success;
}

granit::result model_viewer_app::shutdown_resources() noexcept {
  auto& state = *state_;
  if (state.shutdown_complete)
    return state.shutdown_result;
  state.cancel_requested.store(true, std::memory_order_release);
  state.model_load.cancel();
  state.environment_request.cancel();
  if (state.desc.observer != nullptr)
    state.desc.observer->on_shutdown();
  state.previews.clear(state.ui);
  state.ui.clear_textures();
  state.model_load.reset();
  state.document.clear();
  state.scene_plan = {};
  std::optional<granit::renderer_info> renderer_info;
  std::optional<granit::swapchain_info> swapchain_info;
  if (state.rendering.valid()) {
    renderer_info = state.rendering.renderer_info();
    if (state.rendering.presentation_valid())
      swapchain_info = state.rendering.swapchain_info();
  }
  granit::renderer_resource_stats stats;
  state.shutdown_result =
      state.rendering.valid() ? state.rendering.shutdown(&stats) : granit::result::success;
  state.shutdown_live_resource_count = stats.total_live_count;
  state.shutdown_pending_retirement_count = stats.pending_retirement_count;
  if (state.shutdown_result.ok() && stats.total_live_count != 0)
    state.shutdown_result = granit::result::internal;
  if (state.terminal_result.ok() && !state.desc.profile_output_path.empty()) {
    if (state.profile_samples.size() != 1000 || !renderer_info || !swapchain_info ||
        !write_profile(state.desc.profile_output_path, *renderer_info, *swapchain_info,
                       state.desc.model_location, state.desc.enable_validation, state.desc.show_ui,
                       state.profile_samples)) {
      state.terminal_result = granit::result::internal;
    }
  }
  state.shutdown_complete = true;
  state.phase = viewer_runtime_phase::stopped;
  state.notify(model_viewer_app_status::stopped, "stopped", state.shutdown_result);
  return state.shutdown_result;
}

void model_viewer_app::on_host_shutdown(granit::result reason) noexcept {
  auto result = shutdown_resources();
  if (state_->terminal_result.ok() && reason.failed())
    state_->terminal_result = reason;
  if (state_->terminal_result.ok() && result.failed())
    state_->terminal_result = result;
}

model_viewer_app_status model_viewer_app::status() const noexcept {
  switch (state_->phase) {
  case viewer_runtime_phase::failed:
    return model_viewer_app_status::failed;
  case viewer_runtime_phase::starting:
    return model_viewer_app_status::starting;
  case viewer_runtime_phase::ready:
    return model_viewer_app_status::ready;
  case viewer_runtime_phase::stopped:
    return model_viewer_app_status::stopped;
  default:
    return model_viewer_app_status::loading;
  }
}

unsigned model_viewer_app::input_event_count() const noexcept { return state_->input_event_count; }
unsigned model_viewer_app::loading_frame_count() const noexcept {
  return state_->loading_frame_count;
}
unsigned model_viewer_app::rendered_frame_count() const noexcept {
  return state_->rendered_frame_count;
}
unsigned model_viewer_app::applied_input_count() const noexcept {
  return state_->applied_input_count;
}
unsigned model_viewer_app::resize_count() const noexcept { return state_->resize_count; }
unsigned model_viewer_app::quality_generation() const noexcept {
  return state_->quality_generation;
}
unsigned model_viewer_app::lighting_generation() const noexcept {
  return state_->lighting_generation;
}
unsigned model_viewer_app::asset_status() const noexcept {
  return state_->phase == viewer_runtime_phase::failed ? 3U : (state_->asset_ready ? 2U : 1U);
}
gltf_rendering::scene_upload_progress model_viewer_app::upload_progress() const noexcept {
  return state_->upload_progress_snapshot();
}
std::uint64_t model_viewer_app::shutdown_live_resource_count() const noexcept {
  return state_->shutdown_live_resource_count;
}
std::uint64_t model_viewer_app::shutdown_pending_retirement_count() const noexcept {
  return state_->shutdown_pending_retirement_count;
}
granit::result model_viewer_app::shutdown_result() const noexcept {
  return state_->shutdown_result;
}
granit::result model_viewer_app::configure_render_quality(const render_quality_config& quality) {
  return state_->configure_quality(quality);
}
granit::result model_viewer_app::configure_lighting(float exposure_ev, float environment_intensity,
                                                    float key_light_intensity) {
  if (state_->phase != viewer_runtime_phase::ready || !state_->asset_ready)
    return granit::result::not_ready;
  viewer_change change;
  change.exposure_ev = exposure_ev;
  change.environment_intensity = environment_intensity;
  auto light = state_->document.state().directional_light();
  light.radiance = {key_light_intensity, key_light_intensity, key_light_intensity};
  change.directional_light = light;
  if (state_->document.state().apply(state_->document.scene(), change) !=
      viewer_state_error::none) {
    return granit::result::invalid_argument;
  }
  ++state_->lighting_generation;
  return granit::result::success;
}
granit::result model_viewer_app::cancel_loading() noexcept {
  if (state_->phase != viewer_runtime_phase::scene_prepare &&
      state_->phase != viewer_runtime_phase::gpu_upload &&
      state_->phase != viewer_runtime_phase::pipeline_prepare) {
    return granit::result::not_ready;
  }
  state_->cancel_requested.store(true, std::memory_order_release);
  state_->model_load.cancel();
  return granit::result::success;
}
granit::result
model_viewer_app::query_renderer_status(granit::renderer_status& output) const noexcept {
  return state_->rendering.query_renderer_status(output);
}
float model_viewer_app::exposure_ev() const noexcept {
  return state_->document.state().exposure_ev();
}
float model_viewer_app::environment_intensity() const noexcept {
  return state_->document.state().environment_intensity();
}
float model_viewer_app::key_light_intensity() const noexcept {
  return state_->document.state().directional_light().radiance.x;
}
float model_viewer_app::max_sampler_anisotropy() const noexcept {
  return state_->rendering.valid() ? state_->rendering.renderer_limits().max_sampler_anisotropy
                                   : 0.0F;
}

} // namespace granit::example::model_viewer
