// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application_core.h"

#include <new>
#include <utility>

namespace granit::example::model_viewer {

granit::render_pipeline_render_desc
viewer_frame::render_desc(granit::texture_view_ref output, granit::texture_format output_format,
                          const granit::acquired_frame* frame,
                          granit::canvas_draw_list_ref canvas_list) const noexcept {
  granit::render_pipeline_render_desc desc;
  desc.scene = snapshot.ref();
  desc.output = output;
  desc.output_format = output_format;
  desc.width = width;
  desc.height = height;
  desc.exposure_ev = exposure_ev;
  desc.draw_bindings = draw_bindings;
  desc.frame = frame;
  desc.canvas = canvas_list;
  desc.clear_color = clear_color;
  desc.environment = &environment;
  return desc;
}

granit::result application_core::begin_renderer() noexcept {
  if (phase_ != application_phase::platform_ready)
    return granit::result::invalid_argument;
  phase_ = application_phase::renderer_pending;
  return granit::result::success;
}

granit::result application_core::renderer_ready() noexcept {
  if (phase_ != application_phase::renderer_pending)
    return granit::result::invalid_argument;
  phase_ = application_phase::asset_loading;
  return granit::result::success;
}

granit::result application_core::accept_scene(gltf::scene scene) {
  if (phase_ != application_phase::asset_loading)
    return granit::result::invalid_argument;
  gltf_rendering::scene_plan plan;
  const auto plan_result = gltf_rendering::build_scene_plan(scene, plan);
  if (plan_result != gltf_rendering::scene_plan_error::none) {
    const auto result = plan_result == gltf_rendering::scene_plan_error::out_of_memory
                            ? granit::result::out_of_memory
                            : granit::result::invalid_argument;
    fail(result, "模型查看器 glTF Scene GPU 资源 计划生成失败");
    return result;
  }
  return accept_scene(std::move(scene), std::move(plan));
}

granit::result application_core::accept_scene(gltf::scene scene, gltf_rendering::scene_plan plan) {
  if (phase_ != application_phase::asset_loading)
    return granit::result::invalid_argument;
  try {
    document_.assign(std::move(scene));
    gpu_plan_ = std::move(plan);
    phase_ = application_phase::gpu_upload;
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    fail(granit::result::out_of_memory, "模型查看器 CPU Scene 分配失败");
    return failure_result_;
  }
}

granit::result application_core::upload(granit::renderer_ref renderer,
                                        std::span<const std::byte> environment_bytes,
                                        float sampler_anisotropy,
                                        gltf_rendering::scene_upload_callback progress,
                                        void* progress_user_data) {
  if (phase_ != application_phase::gpu_upload)
    return granit::result::invalid_argument;
  const auto result = scene_resources_.initialize(renderer, document_.scene(), std::move(gpu_plan_),
                                                  sampler_anisotropy, progress, progress_user_data);
  if (result.failed()) {
    fail(result, "模型查看器 glTF Scene GPU 资源 上传失败");
    return result;
  }
  granit::result environment_result;
  if (environment_bytes.empty()) {
    environment_result = environment_.initialize_builtin(renderer);
  } else {
    environment_result = environment_.initialize(renderer, environment_bytes);
  }
  if (environment_result.ok())
    environment_result = environment_.get_info(environment_info_);
  if (environment_result.ok() && !environment_bytes.empty()) {
    viewer_change recommended_lighting;
    recommended_lighting.environment_intensity = environment_info_.environment.intensity;
    recommended_lighting.exposure_ev = environment_info_.recommended_exposure_ev;
    if (document_.apply(recommended_lighting) != viewer_state_error::none)
      environment_result = granit::result::invalid_argument;
  }
  if (environment_result.failed()) {
    scene_resources_.reset();
    fail(environment_result, "模型查看器内建环境上传失败");
    return environment_result;
  }
  phase_ = application_phase::ready;
  return granit::result::success;
}

granit::result application_core::reupload_scene(granit::renderer_ref renderer,
                                                float sampler_anisotropy) {
  if (phase_ != application_phase::ready)
    return granit::result::invalid_argument;
  return scene_resources_.initialize(renderer, document_.scene(), sampler_anisotropy);
}

granit::result application_core::tick(const viewer_document_update& input, viewer_frame& output) {
  if (phase_ != application_phase::ready)
    return granit::result::invalid_argument;
  viewer_document_frame document_frame;
  const auto document_result = document_.update(input, scene_resources_.plan(), document_frame);
  if (document_result.failed())
    return document_result;
  if (input.change.debug_display) {
    const auto debug_result = scene_resources_.update_debug_display(
        static_cast<std::uint32_t>(*input.change.debug_display));
    if (debug_result.failed())
      return debug_result;
  }

  viewer_frame candidate;
  const auto snapshot_result = scene_resources_.create_snapshot(
      std::span{&document_frame.view, 1}, std::span{&document_frame.directional_light, 1}, {}, {},
      candidate.snapshot);
  if (snapshot_result.failed())
    return snapshot_result;
  candidate.width = input.width;
  candidate.height = input.height;
  candidate.exposure_ev = document_frame.exposure_ev;
  candidate.clear_color = document_frame.clear_color;
  candidate.environment = {
      .irradiance = environment_info_.environment.irradiance,
      .prefiltered_environment = environment_info_.environment.prefiltered_environment,
      .brdf_lut = environment_info_.environment.brdf_lut,
      .rotation_radians = environment_info_.environment.rotation_radians,
      .intensity = environment_info_.environment.intensity,
      .prefiltered_max_mip = environment_info_.environment.prefiltered_max_mip,
  };
  candidate.environment.intensity = document_frame.environment_intensity;
  candidate.environment.rotation_radians = document_frame.environment_rotation_radians;
  try {
    candidate.draw_bindings = scene_resources_.draw_bindings();
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  }
  output = std::move(candidate);
  return granit::result::success;
}

void application_core::fail(granit::result result, std::string diagnostic) {
  scene_resources_.reset();
  failure_result_ = result;
  diagnostic_ = std::move(diagnostic);
  phase_ = application_phase::failed;
}

void application_core::reset() noexcept {
  scene_resources_.reset();
  static_cast<void>(environment_.reset());
  environment_info_ = {};
  document_.clear();
  gpu_plan_ = {};
  diagnostic_.clear();
  failure_result_ = granit::result::success;
  phase_ = application_phase::platform_ready;
}

} // namespace granit::example::model_viewer
