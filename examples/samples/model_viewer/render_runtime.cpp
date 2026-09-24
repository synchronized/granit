// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/render_runtime.h"

#include <array>
#include <chrono>
#include <new>
#include <utility>

namespace granit::example::model_viewer {

struct render_runtime::state {
  granit::renderer renderer_owner;
  granit::renderer_info renderer_info;
  granit::renderer_limits renderer_limits;
  granit::surface surface;
  granit::swapchain swapchain;
  granit::swapchain_info swapchain_info;
  application_core* core{};
  granit::frame_context loading_frame_context;
  granit::canvas_draw_list loading_canvas;
  std::array<granit::canvas_draw_list, 3> frame_canvases;
  granit::texture font_texture;
  granit::texture_view font_view;
  granit::sampler font_sampler;
  granit::render_pipeline pipeline;
  std::size_t next_canvas{};
  bool metrics_enabled{};
};

render_runtime::render_runtime() = default;

render_runtime::~render_runtime() { static_cast<void>(shutdown()); }

granit::result render_runtime::initialize_renderer(const granit::renderer_desc& desc,
                                                   application_core& core) noexcept {
  if (state_)
    return granit::result::invalid_argument;
  try {
    auto state = std::make_unique<render_runtime::state>();
    const auto result = state->renderer_owner.initialize(desc);
    if (result.failed())
      return result;
    state->core = &core;
    state_ = std::move(state);
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

granit::result render_runtime::complete_renderer_initialization() noexcept {
  if (!state_)
    return granit::result::not_ready;
  auto result = state_->renderer_owner.get_info(state_->renderer_info);
  if (result.ok())
    result = state_->renderer_owner.get_limits(state_->renderer_limits);
  return result;
}

granit::result render_runtime::initialize_presentation(granit::window& window,
                                                       const granit::swapchain_desc& desc,
                                                       bool enable_ui) noexcept {
  if (!state_ || state_->surface.valid() || state_->swapchain.valid())
    return granit::result::not_ready;
  auto result = window.create_surface(state_->renderer_owner, state_->surface);
  if (result.ok())
    result = state_->swapchain.initialize(state_->renderer_owner, state_->surface, desc);
  if (result.ok())
    result = state_->swapchain.query_info(state_->swapchain_info);
  if (enable_ui) {
    if (result.ok())
      result = state_->loading_frame_context.initialize(state_->renderer_owner);
    if (result.ok())
      result = state_->loading_canvas.initialize(state_->renderer_owner);
    for (auto& canvas : state_->frame_canvases) {
      if (result.ok())
        result = canvas.initialize(state_->renderer_owner);
    }
  }
  return result;
}

granit::result render_runtime::process_renderer_events() noexcept {
  return state_ ? state_->renderer_owner.process_events() : granit::result::not_ready;
}

granit::result
render_runtime::query_renderer_status(granit::renderer_status& status) const noexcept {
  return state_ ? state_->renderer_owner.get_status(status) : granit::result::not_ready;
}

granit::result render_runtime::upload_scene(std::span<const std::byte> environment_bytes,
                                            float sampler_anisotropy,
                                            gpu_scene_upload_callback progress,
                                            void* progress_user_data) {
  return state_ ? state_->core->upload(state_->renderer_owner, environment_bytes,
                                       sampler_anisotropy, progress, progress_user_data)
                : granit::result::not_ready;
}

granit::result render_runtime::render(frame_packet&& packet, frame_execution_result& output) {
  if (!state_ || !state_->pipeline.valid() || !state_->swapchain.valid())
    return granit::result::not_ready;
  granit::acquired_frame frame;
  const auto acquire_begin = std::chrono::steady_clock::now();
  auto result = state_->swapchain.acquire(frame);
  output.acquire_wait_ms =
      std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - acquire_begin)
          .count();
  if (result.failed())
    return result;

  output.needs_recreate = frame.needs_recreate();
  granit::swapchain_backbuffer backbuffer;
  result = state_->swapchain.backbuffer(frame, backbuffer);
  if (result.ok()) {
    granit::canvas_draw_list_ref canvas;
    if (!packet.canvas.empty()) {
      auto& canvas_slot = state_->frame_canvases[state_->next_canvas];
      state_->next_canvas = (state_->next_canvas + 1) % state_->frame_canvases.size();
      result = canvas_slot.clear();
      if (result.ok())
        result = packet.canvas.append_to(canvas_slot);
      if (result.ok())
        canvas = canvas_slot.ref();
    }
    if (result.failed()) {
      static_cast<void>(state_->swapchain.cancel(frame));
      return result;
    }
    result = state_->pipeline.render(
        packet.viewer.render_desc(backbuffer.view, state_->swapchain_info.format, &frame, canvas));
  }
  if (result.failed()) {
    static_cast<void>(state_->swapchain.cancel(frame));
    return result;
  }

  const auto present_begin = std::chrono::steady_clock::now();
  result = state_->swapchain.present(frame);
  output.present_wait_ms =
      std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - present_begin)
          .count();
  output.needs_recreate = output.needs_recreate || frame.needs_recreate();
  if (state_->metrics_enabled) {
    granit::render_pipeline_metrics metrics{};
    const auto metrics_result = state_->pipeline.get_metrics(metrics);
    if (metrics_result.ok()) {
      output.gpu_frame_ms = static_cast<float>(metrics.total_gpu_ns) / 1'000'000.0F;
      output.gpu_timing_available = true;
    } else if (metrics_result == granit::result::unsupported) {
      state_->metrics_enabled = false;
    }
  }
  return result;
}

granit::result render_runtime::render_loading_frame(const imgui::frame_canvas_data& data) noexcept {
  if (!state_ || !state_->loading_frame_context.valid())
    return granit::result::not_ready;
  auto result = state_->loading_canvas.clear();
  if (result.ok())
    result = data.append_to(state_->loading_canvas);
  granit::acquired_frame frame;
  if (result.ok())
    result = state_->swapchain.acquire(frame);
  granit::swapchain_backbuffer backbuffer;
  if (result.ok())
    result = state_->swapchain.backbuffer(frame, backbuffer);
  granit::frame_recording recording;
  if (result.ok())
    result = state_->loading_frame_context.begin(frame, recording);
  if (result.ok()) {
    const auto format = state_->swapchain_info.format;
    result = state_->loading_canvas.record(
        recording.recorder(), {.color = backbuffer.view,
                               .color_format = format,
                               .width = state_->swapchain_info.width,
                               .height = state_->swapchain_info.height,
                               .load_operation = granit::attachment_load_operation::clear,
                               .encode_srgb = format == granit::texture_format::rgba8_unorm ||
                                              format == granit::texture_format::bgra8_unorm,
                               .frame_slot = recording.frame_slot()});
  }
  if (result.ok())
    result = recording.submit();
  if (result.ok())
    result = state_->swapchain.present(frame);
  if (result.failed()) {
    if (recording.valid())
      static_cast<void>(recording.abort());
    if (frame.valid())
      static_cast<void>(state_->swapchain.cancel(frame));
  }
  return result;
}

granit::result render_runtime::finish_loading() noexcept {
  if (!state_)
    return granit::result::not_ready;
  const auto frame_result = state_->loading_frame_context.reset();
  const auto canvas_result = state_->loading_canvas.destroy();
  return frame_result.failed() ? frame_result : canvas_result;
}

granit::result render_runtime::initialize_font_atlas(std::span<const std::byte> pixels,
                                                     std::uint32_t width,
                                                     std::uint32_t height) noexcept {
  if (!state_ || pixels.empty() || width == 0 || height == 0)
    return granit::result::invalid_argument;
  auto result = state_->font_texture.initialize(
      state_->renderer_owner,
      {.format = granit::texture_format::rgba8_unorm,
       .usage = granit::texture_usage::sampled | granit::texture_usage::transfer_destination,
       .width = width,
       .height = height});
  if (result.ok()) {
    result =
        state_->font_texture.write(pixels, {.bytes_per_row = width * 4, .rows_per_image = height},
                                   {.width = width, .height = height});
  }
  if (result.ok())
    result = state_->font_view.initialize(state_->renderer_owner.ref(), state_->font_texture.ref());
  if (result.ok()) {
    result = state_->font_sampler.initialize(state_->renderer_owner,
                                             {.address_u = granit::address_mode::clamp_to_edge,
                                              .address_v = granit::address_mode::clamp_to_edge,
                                              .address_w = granit::address_mode::clamp_to_edge});
  }
  return result;
}

granit::result
render_runtime::initialize_pipeline(const granit::render_pipeline_desc& desc) noexcept {
  render_quality_change_result ignored;
  return change_quality(desc, 1.0F, false, ignored);
}

granit::result render_runtime::change_quality(const granit::render_pipeline_desc& desc,
                                              float sampler_anisotropy, bool reupload_scene,
                                              render_quality_change_result& output) noexcept {
  output = {};
  if (!state_)
    return granit::result::not_ready;
  granit::render_pipeline replacement;
  auto result = replacement.initialize(state_->renderer_owner, desc);
  bool metrics_enabled{};
  if (result.ok()) {
    const auto metrics_result = replacement.enable_metrics();
    if (metrics_result == granit::result::success)
      metrics_enabled = true;
    else if (metrics_result != granit::result::unsupported)
      result = metrics_result;
  }
  if (result.ok() && reupload_scene)
    result = state_->core->reupload_scene(state_->renderer_owner, sampler_anisotropy);
  if (result.ok()) {
    state_->pipeline = std::move(replacement);
    state_->metrics_enabled = metrics_enabled;
    output.scene_reuploaded = reupload_scene;
  }
  return result;
}

granit::result render_runtime::update_material(std::uint32_t material_index,
                                               const material_factor_edit& edit) noexcept {
  return state_ ? state_->core->scene_gpu().update_material_factors(state_->core->cpu_scene(),
                                                                    material_index, edit)
                : granit::result::not_ready;
}

granit::result render_runtime::recreate_swapchain(const granit::swapchain_desc& desc) noexcept {
  if (!state_)
    return granit::result::not_ready;
  auto result = state_->swapchain.recreate(desc);
  if (result.ok())
    result = state_->swapchain.query_info(state_->swapchain_info);
  return result;
}

granit::result render_runtime::recreate_surface(granit::window& window,
                                                const granit::swapchain_desc& desc) noexcept {
  if (!state_)
    return granit::result::not_ready;
  auto result = state_->swapchain.reset();
  if (result.ok())
    result = state_->surface.reset();
  if (result.ok())
    result = window.create_surface(state_->renderer_owner, state_->surface);
  if (result.ok())
    result = state_->swapchain.initialize(state_->renderer_owner, state_->surface, desc);
  if (result.ok())
    result = state_->swapchain.query_info(state_->swapchain_info);
  return result;
}

granit::result
render_runtime::query_resource_stats(granit::renderer_resource_stats& stats) const noexcept {
  return state_ ? state_->renderer_owner.get_resource_stats(stats) : granit::result::not_ready;
}

granit::result render_runtime::shutdown(granit::renderer_resource_stats* final_stats) noexcept {
  if (!state_)
    return granit::result::success;
  granit::result first_failure = granit::result::success;
  const auto collect = [&](granit::result value) {
    if (first_failure.ok() && value.failed())
      first_failure = value;
  };
  collect(state_->pipeline.reset());
  state_->core->reset();
  for (auto& canvas : state_->frame_canvases)
    collect(canvas.destroy());
  collect(state_->loading_canvas.destroy());
  collect(state_->loading_frame_context.reset());
  collect(state_->font_sampler.reset());
  collect(state_->font_view.reset());
  collect(state_->font_texture.reset());
  collect(state_->swapchain.reset());
  collect(state_->surface.reset());
  if (final_stats != nullptr)
    collect(state_->renderer_owner.get_resource_stats(*final_stats));
  collect(state_->renderer_owner.reset());
  state_.reset();
  return first_failure;
}

const granit::renderer_info& render_runtime::renderer_info() const noexcept {
  return state_->renderer_info;
}

const granit::renderer_limits& render_runtime::renderer_limits() const noexcept {
  return state_->renderer_limits;
}

const granit::swapchain_info& render_runtime::swapchain_info() const noexcept {
  return state_->swapchain_info;
}

granit::texture_view_ref render_runtime::font_view() const noexcept {
  return state_ ? state_->font_view.ref() : granit::texture_view_ref{};
}

granit::sampler_ref render_runtime::font_sampler() const noexcept {
  return state_ ? state_->font_sampler.ref() : granit::sampler_ref{};
}

granit::renderer_ref render_runtime::renderer() const noexcept {
  return state_ ? state_->renderer_owner.ref() : granit::renderer_ref{};
}

granit_renderer render_runtime::native_renderer() const noexcept {
  return state_ ? state_->renderer_owner.native_handle() : GRANIT_NULL_HANDLE;
}

granit_swapchain render_runtime::native_swapchain() const noexcept {
  return state_ ? state_->swapchain.native_handle() : GRANIT_NULL_HANDLE;
}

bool render_runtime::valid() const noexcept { return state_ && state_->renderer_owner.valid(); }

} // namespace granit::example::model_viewer
