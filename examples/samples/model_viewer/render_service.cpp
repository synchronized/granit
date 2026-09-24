// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/render_service.h"

#include <new>
#include <vector>

namespace granit::example::model_viewer {

struct render_service::state {
  render_runtime runtime;
  render_task_executor* executor{};
};

render_service::render_service() = default;

render_service::~render_service() {
  if (state_ && state_->executor)
    state_->executor->stop();
}

granit::result render_service::initialize_renderer(render_task_executor& executor,
                                                   const granit::renderer_desc& desc,
                                                   viewer_session& session) noexcept {
  if (state_)
    return granit::result::invalid_argument;
  try {
    auto state = std::make_unique<render_service::state>();
    auto result = state->runtime.initialize_renderer(desc, session);
    if (result.ok()) {
      result = executor.initialize([context = state.get()](auto&& packet, auto& output) {
        return context->runtime.render(std::move(packet), output);
      });
    }
    if (result.failed())
      return result;
    state->executor = &executor;
    state_ = std::move(state);
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  }
}

granit::result render_service::complete_renderer_initialization() noexcept {
  return state_ ? state_->runtime.complete_renderer_initialization() : granit::result::not_ready;
}

granit::result render_service::initialize_presentation(granit::window& window,
                                                       const granit::swapchain_desc& desc,
                                                       bool enable_ui) noexcept {
  return state_ ? state_->runtime.initialize_presentation(window, desc, enable_ui)
                : granit::result::not_ready;
}

granit::result render_service::process_renderer_events() noexcept {
  return state_ ? state_->runtime.process_renderer_events() : granit::result::not_ready;
}

granit::result
render_service::query_renderer_status(granit::renderer_status& status) const noexcept {
  return state_ ? state_->runtime.query_renderer_status(status) : granit::result::not_ready;
}

granit::result render_service::upload_scene(std::span<const std::byte> environment_bytes,
                                            float sampler_anisotropy,
                                            gpu_scene_upload_callback progress,
                                            void* progress_user_data) {
  if (!state_)
    return granit::result::not_ready;
  try {
    std::vector<std::byte> owned(environment_bytes.begin(), environment_bytes.end());
    return state_->executor->run_task([context = state_.get(), bytes = std::move(owned),
                                       sampler_anisotropy, progress, progress_user_data] {
      return context->runtime.upload_scene(bytes, sampler_anisotropy, progress, progress_user_data);
    });
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

granit::result render_service::execute_upload_scene(std::span<const std::byte> environment_bytes,
                                                    float sampler_anisotropy,
                                                    gpu_scene_upload_callback progress,
                                                    void* progress_user_data) {
  return state_ ? state_->runtime.upload_scene(environment_bytes, sampler_anisotropy, progress,
                                               progress_user_data)
                : granit::result::not_ready;
}

granit::result render_service::begin_upload_scene(std::span<const std::byte> environment_bytes,
                                                  float sampler_anisotropy,
                                                  gpu_scene_upload_callback progress,
                                                  void* progress_user_data,
                                                  std::uint64_t& sequence) noexcept {
  if (!state_)
    return granit::result::not_ready;
  try {
    std::vector<std::byte> owned(environment_bytes.begin(), environment_bytes.end());
    return state_->executor->submit_control(
        [context = state_.get(), bytes = std::move(owned), sampler_anisotropy, progress,
         progress_user_data] {
          return context->runtime.upload_scene(bytes, sampler_anisotropy, progress,
                                               progress_user_data);
        },
        sequence);
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

granit::result render_service::submit(frame_packet packet, frame_execution_result& output) {
  return state_ ? state_->executor->submit(std::move(packet), output) : granit::result::not_ready;
}

granit::result render_service::submit_frame(frame_packet packet, std::uint64_t& sequence) noexcept {
  return state_ ? state_->executor->submit_frame(std::move(packet), sequence)
                : granit::result::not_ready;
}

bool render_service::try_take_frame_completion(frame_completion& completion) noexcept {
  return state_ && state_->executor->try_take_frame_completion(completion);
}

bool render_service::try_take_control_completion(render_task_completion& completion) noexcept {
  return state_ && state_->executor->try_take_control_completion(completion);
}

bool render_service::can_submit_frame() const noexcept {
  return state_ && state_->executor->can_submit_frame();
}

void render_service::record_skipped_frame_build() noexcept {
  if (state_)
    state_->executor->record_skipped_frame_build();
}

render_task_queue_stats render_service::query_queue_stats() const noexcept {
  return state_ ? state_->executor->query_queue_stats() : render_task_queue_stats{};
}

granit::result render_service::flush() noexcept {
  return state_ ? state_->executor->flush() : granit::result::not_ready;
}

granit::result render_service::render_loading_frame(const imgui::frame_canvas_data& data) noexcept {
  return state_ ? state_->runtime.render_loading_frame(data) : granit::result::not_ready;
}

granit::result render_service::finish_loading() noexcept {
  return state_ ? state_->executor->run_task(
                      [context = state_.get()] { return context->runtime.finish_loading(); })
                : granit::result::not_ready;
}

granit::result render_service::initialize_font_atlas(std::span<const std::byte> pixels,
                                                     std::uint32_t width,
                                                     std::uint32_t height) noexcept {
  if (!state_ || pixels.empty() || width == 0 || height == 0)
    return granit::result::invalid_argument;
  try {
    std::vector<std::byte> owned(pixels.begin(), pixels.end());
    return state_->executor->run_task(
        [context = state_.get(), bytes = std::move(owned), width, height] {
          return context->runtime.initialize_font_atlas(bytes, width, height);
        });
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

granit::result
render_service::initialize_pipeline(const granit::render_pipeline_desc& desc) noexcept {
  return state_ ? state_->executor->run_task([context = state_.get(), desc] {
    return context->runtime.initialize_pipeline(desc);
  })
                : granit::result::not_ready;
}

granit::result render_service::change_quality(const granit::render_pipeline_desc& desc,
                                              float sampler_anisotropy, bool reupload_scene,
                                              render_quality_change_result& output) noexcept {
  if (!state_)
    return granit::result::not_ready;
  return state_->executor->run_task(
      [context = state_.get(), desc, sampler_anisotropy, reupload_scene, &output] {
        return context->runtime.change_quality(desc, sampler_anisotropy, reupload_scene, output);
      });
}

granit::result render_service::update_material(std::uint32_t material_index,
                                               const material_factor_edit& edit) noexcept {
  return state_ ? state_->executor->run_task([context = state_.get(), material_index, edit] {
    return context->runtime.update_material(material_index, edit);
  })
                : granit::result::not_ready;
}

granit::result render_service::recreate_swapchain(const granit::swapchain_desc& desc) noexcept {
  return state_ ? state_->executor->run_task([context = state_.get(), desc] {
    return context->runtime.recreate_swapchain(desc);
  })
                : granit::result::not_ready;
}

granit::result render_service::recreate_surface(granit::window& window,
                                                const granit::swapchain_desc& desc) noexcept {
  if (!state_)
    return granit::result::not_ready;
  auto result = state_->executor->flush();
  if (result.ok())
    result = state_->runtime.recreate_surface(window, desc);
  return result;
}

granit::result
render_service::query_resource_stats(granit::renderer_resource_stats& stats) const noexcept {
  return state_ ? state_->runtime.query_resource_stats(stats) : granit::result::not_ready;
}

granit::result render_service::shutdown(granit::renderer_resource_stats* final_stats) noexcept {
  if (!state_)
    return granit::result::not_ready;
  const auto result = state_->executor->run_task(
      [context = state_.get(), final_stats] { return context->runtime.shutdown(final_stats); });
  state_->executor->stop();
  return result;
}

const granit::renderer_info& render_service::renderer_info() const noexcept {
  return state_->runtime.renderer_info();
}

const granit::renderer_limits& render_service::renderer_limits() const noexcept {
  return state_->runtime.renderer_limits();
}

const granit::swapchain_info& render_service::swapchain_info() const noexcept {
  return state_->runtime.swapchain_info();
}

granit::texture_view_ref render_service::font_view() const noexcept {
  return state_ ? state_->runtime.font_view() : granit::texture_view_ref{};
}

granit::sampler_ref render_service::font_sampler() const noexcept {
  return state_ ? state_->runtime.font_sampler() : granit::sampler_ref{};
}

granit::renderer_ref render_service::renderer() const noexcept {
  return state_ ? state_->runtime.renderer() : granit::renderer_ref{};
}

granit_renderer render_service::native_renderer() const noexcept {
  return state_ ? state_->runtime.native_renderer() : GRANIT_NULL_HANDLE;
}

granit_swapchain render_service::native_swapchain() const noexcept {
  return state_ ? state_->runtime.native_swapchain() : GRANIT_NULL_HANDLE;
}

bool render_service::valid() const noexcept { return state_ && state_->runtime.valid(); }

bool render_service::presentation_valid() const noexcept {
  return state_ && state_->runtime.presentation_valid();
}

bool render_service::running() const noexcept {
  return state_ && state_->executor && state_->executor->running();
}

} // namespace granit::example::model_viewer
