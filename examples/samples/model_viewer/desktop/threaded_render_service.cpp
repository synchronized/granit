// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "threaded_render_service.h"

#include "model_viewer/render_service.h"
#include "model_viewer/viewer_session.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <new>
#include <thread>
#include <vector>

namespace granit::example::model_viewer::desktop {

struct threaded_render_service_state {
  threaded_render_task_executor executor;
  render_service service;
  gpu_upload_desc upload;
  std::vector<std::byte> upload_environment;
  std::atomic<bool> upload_cancelled{};
  granit::result upload_progress_result{granit::result::success};
  std::uint64_t upload_sequence{};
  unsigned displayed_percentage{40};
  bool upload_active{};
};

namespace {

unsigned gpu_upload_percentage(const gpu_scene_upload_progress& progress) noexcept {
  const auto local = progress.total == 0
                         ? 0U
                         : static_cast<unsigned>(std::min<std::uint64_t>(
                               100, std::uint64_t{progress.completed} * 100 / progress.total));
  unsigned base = 40;
  unsigned span = 2;
  using enum gpu_scene_upload_stage;
  switch (progress.stage) {
  case planning:
    break;
  case geometry:
    base = 42;
    span = 6;
    break;
  case textures:
    base = 48;
    span = 28;
    break;
  case samplers:
    base = 76;
    span = 4;
    break;
  case meshes:
    base = 80;
    span = 6;
    break;
  case materials:
    base = 86;
    span = 8;
    break;
  }
  return base + span * local / 100;
}

bool update_gpu_upload(const gpu_scene_upload_progress& progress, void* user_data) {
  auto& state = *static_cast<threaded_render_service_state*>(user_data);
  if (state.upload_cancelled.load(std::memory_order_acquire))
    return false;
  if (state.upload.progress == nullptr)
    return true;
  const auto target_percentage = gpu_upload_percentage(progress);
  while (state.displayed_percentage < target_percentage) {
    ++state.displayed_percentage;
    const auto result =
        state.upload.progress(state.displayed_percentage, state.upload.progress_user_data);
    if (result.failed()) {
      state.upload_progress_result = result;
      return false;
    }
    if (state.upload_cancelled.load(std::memory_order_acquire))
      return false;
    // Immediate 模式下 Present 可能不节流，保留最小展示时间避免进度瞬间跳过。
    std::this_thread::sleep_for(std::chrono::milliseconds{4});
  }
  return true;
}

granit::result execute_gpu_upload(threaded_render_service_state& state) {
  try {
    const auto result = state.service.execute_upload_scene(
        state.upload.environment_bytes, state.upload.sampler_anisotropy, update_gpu_upload, &state);
    return result == granit::result::not_ready && state.upload_progress_result.failed()
               ? state.upload_progress_result
               : result;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  }
}

} // namespace

threaded_render_service::threaded_render_service() = default;

threaded_render_service::~threaded_render_service() {
  if (state_)
    state_->executor.stop();
}

granit::result threaded_render_service::initialize(granit::window& window,
                                                   const granit::renderer_desc& renderer_desc,
                                                   const granit::swapchain_desc& swapchain_desc,
                                                   viewer_session& session,
                                                   bool enable_ui) noexcept {
  if (state_)
    return granit::result::invalid_argument;
  try {
    auto state = std::make_unique<threaded_render_service_state>();
    auto result = state->service.initialize_renderer(state->executor, renderer_desc, session);
    if (result.ok())
      result = state->service.complete_renderer_initialization();
    if (result.ok())
      result = session.renderer_ready();
    if (result.ok())
      result = state->service.initialize_presentation(window, swapchain_desc, enable_ui);
    if (result.failed())
      return result;
    state_ = std::move(state);
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  }
}

const granit::renderer_info& threaded_render_service::renderer_info() const noexcept {
  return state_->service.renderer_info();
}

const granit::renderer_limits& threaded_render_service::renderer_limits() const noexcept {
  return state_->service.renderer_limits();
}

const granit::swapchain_info& threaded_render_service::swapchain_info() const noexcept {
  return state_->service.swapchain_info();
}

granit::result threaded_render_service::begin_gpu_upload(const gpu_upload_desc& desc) noexcept {
  if (!state_ || state_->upload_active)
    return granit::result::not_ready;
  try {
    state_->upload_environment.assign(desc.environment_bytes.begin(), desc.environment_bytes.end());
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
  state_->upload = desc;
  state_->upload.environment_bytes = state_->upload_environment;
  state_->upload_cancelled.store(false, std::memory_order_release);
  state_->upload_progress_result = granit::result::success;
  state_->displayed_percentage = 40;
  const auto result = state_->executor.submit_task(
      [context = state_.get()] { return execute_gpu_upload(*context); }, state_->upload_sequence);
  state_->upload_active = result.ok();
  return result;
}

bool threaded_render_service::try_finish_gpu_upload(granit::result& status) noexcept {
  if (!state_ || !state_->upload_active)
    return false;
  render_task_completion completion;
  if (!state_->executor.try_take_task_completion(completion))
    return false;
  status =
      completion.sequence == state_->upload_sequence ? completion.status : granit::result::internal;
  state_->upload_active = false;
  state_->upload = {};
  state_->upload_environment.clear();
  return true;
}

void threaded_render_service::cancel_gpu_upload() noexcept {
  if (state_)
    state_->upload_cancelled.store(true, std::memory_order_release);
}

granit::result
threaded_render_service::render_loading_frame(const imgui::frame_canvas_data& data) noexcept {
  return state_ ? state_->service.render_loading_frame(data) : granit::result::not_ready;
}

granit::result threaded_render_service::finish_loading() noexcept {
  return state_ ? state_->service.finish_loading() : granit::result::not_ready;
}

granit::result threaded_render_service::initialize_font_atlas(std::span<const std::byte> pixels,
                                                              std::uint32_t width,
                                                              std::uint32_t height) noexcept {
  if (!state_ || pixels.empty() || width == 0 || height == 0)
    return granit::result::invalid_argument;
  return state_->service.initialize_font_atlas(pixels, width, height);
}

granit::texture_view_ref threaded_render_service::font_view() const noexcept {
  return state_ ? state_->service.font_view() : granit::texture_view_ref{};
}

granit::sampler_ref threaded_render_service::font_sampler() const noexcept {
  return state_ ? state_->service.font_sampler() : granit::sampler_ref{};
}

granit::result
threaded_render_service::initialize_pipeline(const granit::render_pipeline_desc& desc) noexcept {
  if (!state_ || state_->upload_active)
    return granit::result::not_ready;
  return state_->service.initialize_pipeline(desc);
}

granit::result
threaded_render_service::recreate_swapchain(const granit::swapchain_desc& desc) noexcept {
  if (!state_ || state_->upload_active)
    return granit::result::not_ready;
  return state_->service.recreate_swapchain(desc);
}

granit::result
threaded_render_service::recreate_surface(granit::window& window,
                                          const granit::swapchain_desc& desc) noexcept {
  if (!state_ || state_->upload_active)
    return granit::result::not_ready;
  return state_->service.recreate_surface(window, desc);
}

granit::result threaded_render_service::change_quality(const granit::render_pipeline_desc& desc,
                                                       float sampler_anisotropy,
                                                       bool reupload_scene,
                                                       quality_change_result& output) noexcept {
  if (!state_ || state_->upload_active)
    return granit::result::not_ready;
  render_quality_change_result runtime_output;
  const auto result =
      state_->service.change_quality(desc, sampler_anisotropy, reupload_scene, runtime_output);
  output = {.scene_reuploaded = runtime_output.scene_reuploaded};
  return result;
}

granit::result threaded_render_service::update_material(std::uint32_t material_index,
                                                        const material_factor_edit& edit) noexcept {
  if (!state_ || state_->upload_active)
    return granit::result::not_ready;
  return state_->service.update_material(material_index, edit);
}

bool threaded_render_service::can_submit_frame() const noexcept {
  return state_ && !state_->upload_active && state_->executor.can_submit_frame();
}

void threaded_render_service::record_skipped_frame_build() noexcept {
  if (state_)
    state_->executor.record_skipped_frame_build();
}

granit::result threaded_render_service::submit(frame_packet packet,
                                               std::uint64_t& sequence) noexcept {
  return state_ && !state_->upload_active ? state_->executor.submit(std::move(packet), sequence)
                                          : granit::result::not_ready;
}

bool threaded_render_service::try_take_completion(frame_completion& completion) noexcept {
  return state_ && state_->executor.try_take_completion(completion);
}

render_task_queue_stats threaded_render_service::query_queue_stats() const noexcept {
  return state_ ? state_->executor.query_queue_stats() : render_task_queue_stats{};
}

granit::result threaded_render_service::flush() noexcept {
  return state_ ? state_->executor.flush() : granit::result::not_ready;
}

granit::result threaded_render_service::shutdown() noexcept {
  if (!state_)
    return granit::result::not_ready;
  if (state_->upload_active) {
    cancel_gpu_upload();
    static_cast<void>(state_->executor.flush());
    granit::result upload_result;
    static_cast<void>(try_finish_gpu_upload(upload_result));
  }
  return state_->service.shutdown();
}

bool threaded_render_service::running() const noexcept {
  return state_ && state_->executor.running();
}

} // namespace granit::example::model_viewer::desktop
