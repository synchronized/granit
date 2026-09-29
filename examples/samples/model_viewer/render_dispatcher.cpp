// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/render_dispatcher.h"

#include <new>
#include <vector>

namespace granit::example::model_viewer {

struct render_dispatcher::state {
  viewer_renderer runtime;
  render_execution_policy* executor{};
};

render_dispatcher::render_dispatcher() = default;

render_dispatcher::~render_dispatcher() {
  if (state_ && state_->executor)
    state_->executor->stop();
}

granit::result render_dispatcher::initialize_renderer(render_execution_policy& executor,
                                                      const granit::renderer_desc& desc) noexcept {
  if (state_)
    return granit::result::invalid_argument;
  try {
    auto state = std::make_unique<render_dispatcher::state>();
    auto result = executor.initialize([context = state.get()](auto&& packet, auto& output) {
      return context->runtime.render(std::move(packet), output);
    });
    if (result.failed())
      return result;
    state->executor = &executor;
    state_ = std::move(state);
    result = executor.run_task(
        [context = state_.get(), desc] { return context->runtime.initialize_renderer(desc); });
    if (result.failed()) {
      executor.stop();
      state_.reset();
      return result;
    }
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  }
}

granit::result render_dispatcher::complete_renderer_initialization() noexcept {
  return state_ ? state_->executor->run_task([context = state_.get()] {
    return context->runtime.complete_renderer_initialization();
  })
                : granit::result::not_ready;
}

granit::result render_dispatcher::initialize_presentation(granit::window& window,
                                                          const granit::swapchain_desc& desc,
                                                          bool enable_ui) noexcept {
  return state_ ? state_->executor->run_task([context = state_.get(), &window, desc, enable_ui] {
    return context->runtime.initialize_presentation(window, desc, enable_ui);
  })
                : granit::result::not_ready;
}

granit::result render_dispatcher::process_renderer_events() noexcept {
  return state_ ? state_->executor->run_task([context = state_.get()] {
    return context->runtime.process_renderer_events();
  })
                : granit::result::not_ready;
}

granit::result
render_dispatcher::query_renderer_status(granit::renderer_status& status) const noexcept {
  return state_ ? state_->executor->run_task([context = state_.get(), &status] {
    return context->runtime.query_renderer_status(status);
  })
                : granit::result::not_ready;
}

granit::result render_dispatcher::upload_scene(gltf::scene scene, gltf_rendering::scene_plan plan,
                                               std::span<const std::byte> environment_bytes,
                                               float sampler_anisotropy,
                                               gltf_rendering::scene_upload_callback progress,
                                               void* progress_user_data) {
  if (!state_)
    return granit::result::not_ready;
  try {
    std::vector<std::byte> owned(environment_bytes.begin(), environment_bytes.end());
    return state_->executor->run_task([context = state_.get(), scene = std::move(scene),
                                       plan = std::move(plan), bytes = std::move(owned),
                                       sampler_anisotropy, progress, progress_user_data]() mutable {
      return context->runtime.upload_scene(std::move(scene), std::move(plan), bytes,
                                           sampler_anisotropy, progress, progress_user_data);
    });
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

granit::result render_dispatcher::execute_upload_scene(
    gltf::scene scene, gltf_rendering::scene_plan plan,
    std::span<const std::byte> environment_bytes, float sampler_anisotropy,
    gltf_rendering::scene_upload_callback progress, void* progress_user_data) {
  return state_ ? state_->runtime.upload_scene(std::move(scene), std::move(plan), environment_bytes,
                                               sampler_anisotropy, progress, progress_user_data)
                : granit::result::not_ready;
}

granit::result render_dispatcher::begin_upload_scene(gltf::scene scene,
                                                     gltf_rendering::scene_plan plan,
                                                     std::span<const std::byte> environment_bytes,
                                                     float sampler_anisotropy,
                                                     gltf_rendering::scene_upload_callback progress,
                                                     void* progress_user_data,
                                                     std::uint64_t& sequence) noexcept {
  if (!state_)
    return granit::result::not_ready;
  try {
    std::vector<std::byte> owned(environment_bytes.begin(), environment_bytes.end());
    return state_->executor->submit_control(
        [context = state_.get(), scene = std::move(scene), plan = std::move(plan),
         bytes = std::move(owned), sampler_anisotropy, progress, progress_user_data]() mutable {
          return context->runtime.upload_scene(std::move(scene), std::move(plan), bytes,
                                               sampler_anisotropy, progress, progress_user_data);
        },
        sequence);
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

granit::result render_dispatcher::submit(frame_packet packet, frame_execution_result& output) {
  return state_ ? state_->executor->submit(std::move(packet), output) : granit::result::not_ready;
}

granit::result render_dispatcher::submit_frame(frame_packet packet,
                                               std::uint64_t& sequence) noexcept {
  return state_ ? state_->executor->submit_frame(std::move(packet), sequence)
                : granit::result::not_ready;
}

bool render_dispatcher::try_take_frame_completion(frame_completion& completion) noexcept {
  return state_ && state_->executor->try_take_frame_completion(completion);
}

bool render_dispatcher::try_take_control_completion(render_task_completion& completion) noexcept {
  return state_ && state_->executor->try_take_control_completion(completion);
}

bool render_dispatcher::can_submit_frame() const noexcept {
  return state_ && state_->executor->can_submit_frame();
}

void render_dispatcher::record_skipped_frame_build() noexcept {
  if (state_)
    state_->executor->record_skipped_frame_build();
}

render_task_queue_stats render_dispatcher::query_queue_stats() const noexcept {
  return state_ ? state_->executor->query_queue_stats() : render_task_queue_stats{};
}

granit::result render_dispatcher::flush() noexcept {
  return state_ ? state_->executor->flush() : granit::result::not_ready;
}

granit::result
render_dispatcher::render_loading_frame(const imgui::frame_canvas_data& data) noexcept {
  if (!state_)
    return granit::result::not_ready;
  try {
    auto owned = data;
    return state_->executor->run_task([context = state_.get(), data = std::move(owned)] {
      return context->runtime.render_loading_frame(data);
    });
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

granit::result render_dispatcher::finish_loading() noexcept {
  return state_ ? state_->executor->run_task(
                      [context = state_.get()] { return context->runtime.finish_loading(); })
                : granit::result::not_ready;
}

granit::result render_dispatcher::initialize_font_atlas(std::span<const std::byte> pixels,
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
render_dispatcher::initialize_pipeline(const granit::render_pipeline_desc& desc) noexcept {
  return state_ ? state_->executor->run_task([context = state_.get(), desc] {
    return context->runtime.initialize_pipeline(desc);
  })
                : granit::result::not_ready;
}

granit::result render_dispatcher::begin_pipeline_prepare(granit::texture_format color_format,
                                                         granit::sample_count samples) noexcept {
  return state_ ? state_->executor->run_task([context = state_.get(), color_format, samples] {
    return context->runtime.begin_pipeline_prepare(color_format, samples);
  })
                : granit::result::not_ready;
}

granit::result render_dispatcher::poll_pipeline_prepare() noexcept {
  return state_ ? state_->executor->run_task(
                      [context = state_.get()] { return context->runtime.poll_pipeline_prepare(); })
                : granit::result::not_ready;
}

granit::result render_dispatcher::change_quality(const granit::render_pipeline_desc& desc,
                                                 float sampler_anisotropy, bool reupload_scene,
                                                 render_quality_change_result& output) noexcept {
  if (!state_)
    return granit::result::not_ready;
  return state_->executor->run_task(
      [context = state_.get(), desc, sampler_anisotropy, reupload_scene, &output] {
        return context->runtime.change_quality(desc, sampler_anisotropy, reupload_scene, output);
      });
}

granit::result
render_dispatcher::update_material(std::uint32_t material_index,
                                   const gltf_rendering::material_factor_update& edit) noexcept {
  return state_ ? state_->executor->run_task([context = state_.get(), material_index, edit] {
    return context->runtime.update_material(material_index, edit);
  })
                : granit::result::not_ready;
}

granit::result render_dispatcher::update_debug_display(std::uint32_t mode) noexcept {
  return state_ ? state_->executor->run_task([context = state_.get(), mode] {
    return context->runtime.update_debug_display(mode);
  })
                : granit::result::not_ready;
}

granit::result render_dispatcher::texture_binding(const gltf::texture_reference& reference,
                                                  bool srgb, granit::texture_view_ref& view,
                                                  granit::sampler_ref& sampler) noexcept {
  return state_ ? state_->executor->run_task(
                      [context = state_.get(), &reference, srgb, &view, &sampler] {
                        return context->runtime.texture_binding(reference, srgb, view, sampler);
                      })
                : granit::result::not_ready;
}

granit::result render_dispatcher::recreate_swapchain(const granit::swapchain_desc& desc) noexcept {
  return state_ ? state_->executor->run_task([context = state_.get(), desc] {
    return context->runtime.recreate_swapchain(desc);
  })
                : granit::result::not_ready;
}

granit::result render_dispatcher::recreate_surface(granit::window& window,
                                                   const granit::swapchain_desc& desc) noexcept {
  if (!state_)
    return granit::result::not_ready;
  auto result = state_->executor->flush();
  if (result.ok())
    result = state_->executor->run_task([context = state_.get(), &window, desc] {
      return context->runtime.recreate_surface(window, desc);
    });
  return result;
}

granit::result
render_dispatcher::query_resource_stats(granit::renderer_resource_stats& stats) const noexcept {
  return state_ ? state_->executor->run_task([context = state_.get(), &stats] {
    return context->runtime.query_resource_stats(stats);
  })
                : granit::result::not_ready;
}

granit::result render_dispatcher::shutdown(granit::renderer_resource_stats* final_stats) noexcept {
  if (!state_)
    return granit::result::not_ready;
  const auto result = state_->executor->run_task(
      [context = state_.get(), final_stats] { return context->runtime.shutdown(final_stats); });
  state_->executor->stop();
  return result;
}

const granit::renderer_info& render_dispatcher::renderer_info() const noexcept {
  return state_->runtime.renderer_info();
}

const granit::renderer_limits& render_dispatcher::renderer_limits() const noexcept {
  return state_->runtime.renderer_limits();
}

const granit::swapchain_info& render_dispatcher::swapchain_info() const noexcept {
  return state_->runtime.swapchain_info();
}

granit::texture_view_ref render_dispatcher::font_view() const noexcept {
  return state_ ? state_->runtime.font_view() : granit::texture_view_ref{};
}

granit::sampler_ref render_dispatcher::font_sampler() const noexcept {
  return state_ ? state_->runtime.font_sampler() : granit::sampler_ref{};
}

granit::renderer_ref render_dispatcher::renderer() const noexcept {
  return state_ ? state_->runtime.renderer() : granit::renderer_ref{};
}

const granit::environment_map_info& render_dispatcher::environment_info() const noexcept {
  return state_->runtime.environment_info();
}

granit_renderer render_dispatcher::native_renderer() const noexcept {
  return state_ ? state_->runtime.native_renderer() : GRANIT_NULL_HANDLE;
}

granit_swapchain render_dispatcher::native_swapchain() const noexcept {
  return state_ ? state_->runtime.native_swapchain() : GRANIT_NULL_HANDLE;
}

bool render_dispatcher::valid() const noexcept { return state_ && state_->runtime.valid(); }

bool render_dispatcher::presentation_valid() const noexcept {
  return state_ && state_->runtime.presentation_valid();
}

bool render_dispatcher::running() const noexcept {
  return state_ && state_->executor && state_->executor->running();
}

} // namespace granit::example::model_viewer
