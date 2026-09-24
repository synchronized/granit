// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "render_service.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <new>
#include <thread>

namespace granit::example::model_viewer::desktop {

struct render_service_state {
  granit::renderer renderer_owner;
  granit::renderer_info renderer_info;
  granit::renderer_limits renderer_limits;
  granit::surface surface_owner;
  granit::swapchain swapchain_owner;
  granit::swapchain_info swapchain_info_owner;
  granit::renderer_ref renderer;
  granit::swapchain* swapchain{};
  granit::swapchain_info* swapchain_info{};
  application_core* core{};
  granit::frame_context loading_frame_context;
  granit::canvas_draw_list loading_canvas;
  std::array<granit::canvas_draw_list, 3> frame_canvases;
  granit::texture font_texture;
  granit::texture_view font_view;
  granit::sampler font_sampler;
  granit::render_pipeline pipeline;
  threaded_frame_executor executor;
  std::size_t next_canvas{};
  bool metrics_enabled{};

  gpu_upload_desc upload;
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
  auto& state = *static_cast<render_service_state*>(user_data);
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

granit::result execute_gpu_upload(void* user_data) {
  auto& state = *static_cast<render_service_state*>(user_data);
  try {
    const auto result =
        state.core->upload(state.renderer, state.upload.environment_bytes,
                           state.upload.sampler_anisotropy, update_gpu_upload, &state);
    return result == granit::result::not_ready && state.upload_progress_result.failed()
               ? state.upload_progress_result
               : result;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  }
}

granit::result execute_frame(frame_packet&& packet, frame_execution_result& output,
                             void* user_data) {
  auto& state = *static_cast<render_service_state*>(user_data);
  granit::acquired_frame frame;
  const auto acquire_begin = std::chrono::steady_clock::now();
  auto result = state.swapchain->acquire(frame);
  output.acquire_wait_ms =
      std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - acquire_begin)
          .count();
  if (result.failed())
    return result;

  output.needs_recreate = frame.needs_recreate();
  granit::swapchain_backbuffer backbuffer;
  result = state.swapchain->backbuffer(frame, backbuffer);
  if (result.ok()) {
    granit::canvas_draw_list_ref canvas;
    if (!packet.canvas.empty()) {
      auto& canvas_slot = state.frame_canvases[state.next_canvas];
      state.next_canvas = (state.next_canvas + 1) % state.frame_canvases.size();
      result = canvas_slot.clear();
      if (result.ok())
        result = packet.canvas.append_to(canvas_slot);
      if (result.ok())
        canvas = canvas_slot.ref();
    }
    if (result.failed()) {
      static_cast<void>(state.swapchain->cancel(frame));
      return result;
    }
    result = state.pipeline.render(
        packet.viewer.render_desc(backbuffer.view, state.swapchain_info->format, &frame, canvas));
  }
  if (result.failed()) {
    static_cast<void>(state.swapchain->cancel(frame));
    return result;
  }

  const auto present_begin = std::chrono::steady_clock::now();
  result = state.swapchain->present(frame);
  output.present_wait_ms =
      std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - present_begin)
          .count();
  output.needs_recreate = output.needs_recreate || frame.needs_recreate();
  if (state.metrics_enabled) {
    granit::render_pipeline_metrics metrics{};
    const auto metrics_result = state.pipeline.get_metrics(metrics);
    if (metrics_result.ok()) {
      output.gpu_frame_ms = static_cast<float>(metrics.total_gpu_ns) / 1'000'000.0F;
      output.gpu_timing_available = true;
    } else if (metrics_result == granit::result::unsupported) {
      state.metrics_enabled = false;
    }
  }
  return result;
}

struct pipeline_context {
  render_service_state* state{};
  granit::render_pipeline_desc desc;
  float sampler_anisotropy{1.0F};
  bool reupload_scene{};
  bool metrics_enabled{};
};

granit::result replace_pipeline(void* user_data) {
  auto& context = *static_cast<pipeline_context*>(user_data);
  granit::render_pipeline replacement;
  auto result = replacement.initialize(context.state->renderer, context.desc);
  if (result.failed())
    return result;
  const auto metrics_result = replacement.enable_metrics();
  if (metrics_result == granit::result::success)
    context.metrics_enabled = true;
  else if (metrics_result != granit::result::unsupported)
    return metrics_result;
  if (context.reupload_scene) {
    result =
        context.state->core->reupload_scene(context.state->renderer, context.sampler_anisotropy);
    if (result.failed())
      return result;
  }
  context.state->pipeline = std::move(replacement);
  context.state->metrics_enabled = context.metrics_enabled;
  return granit::result::success;
}

struct swapchain_context {
  render_service_state* state{};
  granit::swapchain_desc desc;
};

granit::result recreate_swapchain(void* user_data) {
  auto& context = *static_cast<swapchain_context*>(user_data);
  auto result = context.state->swapchain->recreate(context.desc);
  if (result.ok())
    result = context.state->swapchain->query_info(*context.state->swapchain_info);
  return result;
}

struct material_context {
  render_service_state* state{};
  std::uint32_t material_index{gltf::invalid_index};
  material_factor_edit edit;
};

granit::result update_material(void* user_data) {
  auto& context = *static_cast<material_context*>(user_data);
  return context.state->core->scene_gpu().update_material_factors(
      context.state->core->cpu_scene(), context.material_index, context.edit);
}

struct shutdown_context {
  render_service_state* state{};
};

struct font_atlas_context {
  render_service_state* state{};
  std::span<const std::byte> pixels;
  std::uint32_t width{};
  std::uint32_t height{};
};

granit::result initialize_font_atlas(void* user_data) {
  auto& context = *static_cast<font_atlas_context*>(user_data);
  auto& state = *context.state;
  auto result = state.font_texture.initialize(
      state.renderer,
      {.format = granit::texture_format::rgba8_unorm,
       .usage = granit::texture_usage::sampled | granit::texture_usage::transfer_destination,
       .width = context.width,
       .height = context.height});
  if (result.ok()) {
    result = state.font_texture.write(
        context.pixels, {.bytes_per_row = context.width * 4, .rows_per_image = context.height},
        {.width = context.width, .height = context.height});
  }
  if (result.ok())
    result = state.font_view.initialize(state.renderer, state.font_texture.ref());
  if (result.ok()) {
    result = state.font_sampler.initialize(state.renderer,
                                           {.address_u = granit::address_mode::clamp_to_edge,
                                            .address_v = granit::address_mode::clamp_to_edge,
                                            .address_w = granit::address_mode::clamp_to_edge});
  }
  return result;
}

granit::result finish_loading(void* user_data) {
  auto& state = *static_cast<render_service_state*>(user_data);
  const auto frame_result = state.loading_frame_context.reset();
  const auto canvas_result = state.loading_canvas.destroy();
  return frame_result.failed() ? frame_result : canvas_result;
}

granit::result shutdown_renderer(void* user_data) {
  auto& context = *static_cast<shutdown_context*>(user_data);
  granit::result first_failure = granit::result::success;
  const auto collect = [&](granit::result value) {
    if (first_failure.ok() && value.failed())
      first_failure = value;
  };
  collect(context.state->pipeline.reset());
  context.state->core->reset();
  for (auto& canvas : context.state->frame_canvases)
    collect(canvas.destroy());
  collect(context.state->loading_canvas.destroy());
  collect(context.state->loading_frame_context.reset());
  collect(context.state->font_sampler.reset());
  collect(context.state->font_view.reset());
  collect(context.state->font_texture.reset());
  collect(context.state->swapchain->reset());
  collect(context.state->surface_owner.reset());
  collect(context.state->renderer_owner.reset());
  return first_failure;
}

} // namespace

render_service::render_service() = default;

render_service::~render_service() {
  if (state_)
    state_->executor.stop();
}

granit::result render_service::initialize(granit::window& window,
                                          const granit::renderer_desc& renderer_desc,
                                          const granit::swapchain_desc& swapchain_desc,
                                          application_core& core, bool enable_ui) noexcept {
  if (state_)
    return granit::result::invalid_argument;
  try {
    auto state = std::make_unique<render_service_state>();
    auto result = state->renderer_owner.initialize(renderer_desc);
    if (result.ok())
      result = state->renderer_owner.get_info(state->renderer_info);
    if (result.ok())
      result = state->renderer_owner.get_limits(state->renderer_limits);
    if (result.ok())
      result = core.renderer_ready();
    if (result.ok())
      result = window.create_surface(state->renderer_owner, state->surface_owner);
    if (result.ok())
      result = state->swapchain_owner.initialize(state->renderer_owner, state->surface_owner,
                                                 swapchain_desc);
    if (result.ok())
      result = state->swapchain_owner.query_info(state->swapchain_info_owner);
    state->renderer = state->renderer_owner.ref();
    state->swapchain = &state->swapchain_owner;
    state->swapchain_info = &state->swapchain_info_owner;
    state->core = &core;
    if (enable_ui) {
      if (result.ok())
        result = state->loading_frame_context.initialize(state->renderer);
      if (result.ok())
        result = state->loading_canvas.initialize(state->renderer);
      for (auto& canvas : state->frame_canvases) {
        if (result.ok())
          result = canvas.initialize(state->renderer);
      }
    }
    if (result.ok())
      result = state->executor.initialize(execute_frame, state.get());
    if (result.failed())
      return result;
    state_ = std::move(state);
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  }
}

const granit::renderer_info& render_service::renderer_info() const noexcept {
  return state_->renderer_info;
}

const granit::renderer_limits& render_service::renderer_limits() const noexcept {
  return state_->renderer_limits;
}

const granit::swapchain_info& render_service::swapchain_info() const noexcept {
  return state_->swapchain_info_owner;
}

granit::result render_service::begin_gpu_upload(const gpu_upload_desc& desc) noexcept {
  if (!state_ || state_->upload_active)
    return granit::result::not_ready;
  state_->upload = desc;
  state_->upload_cancelled.store(false, std::memory_order_release);
  state_->upload_progress_result = granit::result::success;
  state_->displayed_percentage = 40;
  const auto result =
      state_->executor.submit_command(execute_gpu_upload, state_.get(), state_->upload_sequence);
  state_->upload_active = result.ok();
  return result;
}

bool render_service::try_finish_gpu_upload(granit::result& status) noexcept {
  if (!state_ || !state_->upload_active)
    return false;
  render_command_completion completion;
  if (!state_->executor.try_take_command_completion(completion))
    return false;
  if (completion.sequence != state_->upload_sequence) {
    status = granit::result::internal;
  } else {
    status = completion.status;
  }
  state_->upload_active = false;
  state_->upload = {};
  return true;
}

void render_service::cancel_gpu_upload() noexcept {
  if (state_)
    state_->upload_cancelled.store(true, std::memory_order_release);
}

granit::result render_service::render_loading_frame(const imgui::frame_canvas_data& data) noexcept {
  if (!state_ || !state_->loading_frame_context.valid())
    return granit::result::not_ready;
  auto result = state_->loading_canvas.clear();
  if (result.ok())
    result = data.append_to(state_->loading_canvas);
  granit::acquired_frame frame;
  if (result.ok())
    result = state_->swapchain->acquire(frame);
  granit::swapchain_backbuffer backbuffer;
  if (result.ok())
    result = state_->swapchain->backbuffer(frame, backbuffer);
  granit::frame_recording recording;
  if (result.ok())
    result = state_->loading_frame_context.begin(frame, recording);
  if (result.ok()) {
    const auto format = state_->swapchain_info->format;
    result = state_->loading_canvas.record(
        recording.recorder(), {.color = backbuffer.view,
                               .color_format = format,
                               .width = state_->swapchain_info->width,
                               .height = state_->swapchain_info->height,
                               .load_operation = granit::attachment_load_operation::clear,
                               .encode_srgb = format == granit::texture_format::rgba8_unorm ||
                                              format == granit::texture_format::bgra8_unorm,
                               .frame_slot = recording.frame_slot()});
  }
  if (result.ok())
    result = recording.submit();
  if (result.ok())
    result = state_->swapchain->present(frame);
  if (result.failed()) {
    if (recording.valid())
      static_cast<void>(recording.abort());
    if (frame.valid())
      static_cast<void>(state_->swapchain->cancel(frame));
  }
  return result;
}

granit::result render_service::finish_loading() noexcept {
  if (!state_)
    return granit::result::not_ready;
  return state_->executor.run_command(desktop::finish_loading, state_.get());
}

granit::result render_service::initialize_font_atlas(std::span<const std::byte> pixels,
                                                     std::uint32_t width,
                                                     std::uint32_t height) noexcept {
  if (!state_ || pixels.empty() || width == 0 || height == 0)
    return granit::result::invalid_argument;
  font_atlas_context context{
      .state = state_.get(), .pixels = pixels, .width = width, .height = height};
  return state_->executor.run_command(desktop::initialize_font_atlas, &context);
}

granit::texture_view_ref render_service::font_view() const noexcept {
  return state_ ? state_->font_view.ref() : granit::texture_view_ref{};
}

granit::sampler_ref render_service::font_sampler() const noexcept {
  return state_ ? state_->font_sampler.ref() : granit::sampler_ref{};
}

granit::result
render_service::initialize_pipeline(const granit::render_pipeline_desc& desc) noexcept {
  if (!state_ || state_->upload_active)
    return granit::result::not_ready;
  pipeline_context context{.state = state_.get(), .desc = desc};
  return state_->executor.run_command(replace_pipeline, &context);
}

granit::result render_service::recreate_swapchain(const granit::swapchain_desc& desc) noexcept {
  if (!state_ || state_->upload_active)
    return granit::result::not_ready;
  swapchain_context context{.state = state_.get(), .desc = desc};
  return state_->executor.run_command(desktop::recreate_swapchain, &context);
}

granit::result render_service::recreate_surface(granit::window& window,
                                                const granit::swapchain_desc& desc) noexcept {
  if (!state_ || state_->upload_active)
    return granit::result::not_ready;
  auto result = state_->executor.flush();
  if (result.ok())
    result = state_->swapchain_owner.reset();
  if (result.ok())
    result = state_->surface_owner.reset();
  if (result.ok())
    result = window.create_surface(state_->renderer_owner, state_->surface_owner);
  if (result.ok()) {
    result =
        state_->swapchain_owner.initialize(state_->renderer_owner, state_->surface_owner, desc);
  }
  if (result.ok())
    result = state_->swapchain_owner.query_info(state_->swapchain_info_owner);
  return result;
}

granit::result render_service::change_quality(const granit::render_pipeline_desc& desc,
                                              float sampler_anisotropy, bool reupload_scene,
                                              quality_change_result& output) noexcept {
  if (!state_ || state_->upload_active)
    return granit::result::not_ready;
  pipeline_context context{.state = state_.get(),
                           .desc = desc,
                           .sampler_anisotropy = sampler_anisotropy,
                           .reupload_scene = reupload_scene};
  const auto result = state_->executor.run_command(replace_pipeline, &context);
  output = {.scene_reuploaded = result.ok() && reupload_scene};
  return result;
}

granit::result render_service::update_material(std::uint32_t material_index,
                                               const material_factor_edit& edit) noexcept {
  if (!state_ || state_->upload_active)
    return granit::result::not_ready;
  material_context context{.state = state_.get(), .material_index = material_index, .edit = edit};
  return state_->executor.run_command(desktop::update_material, &context);
}

bool render_service::can_submit_frame() const noexcept {
  return state_ && !state_->upload_active && state_->executor.can_submit_frame();
}

void render_service::record_skipped_frame_build() noexcept {
  if (state_)
    state_->executor.record_skipped_frame_build();
}

granit::result render_service::submit(frame_packet packet, std::uint64_t& sequence) noexcept {
  return state_ && !state_->upload_active ? state_->executor.submit(std::move(packet), sequence)
                                          : granit::result::not_ready;
}

bool render_service::try_take_completion(frame_completion& completion) noexcept {
  return state_ && state_->executor.try_take_completion(completion);
}

render_task_queue_stats render_service::query_queue_stats() const noexcept {
  return state_ ? state_->executor.query_queue_stats() : render_task_queue_stats{};
}

granit::result render_service::flush() noexcept {
  return state_ ? state_->executor.flush() : granit::result::not_ready;
}

granit::result render_service::shutdown() noexcept {
  if (!state_)
    return granit::result::not_ready;
  if (state_->upload_active) {
    cancel_gpu_upload();
    static_cast<void>(state_->executor.flush());
    granit::result upload_result;
    static_cast<void>(try_finish_gpu_upload(upload_result));
  }
  shutdown_context context{.state = state_.get()};
  const auto result = state_->executor.run_command(shutdown_renderer, &context);
  state_->executor.stop();
  return result;
}

bool render_service::running() const noexcept { return state_ && state_->executor.running(); }

} // namespace granit::example::model_viewer::desktop
