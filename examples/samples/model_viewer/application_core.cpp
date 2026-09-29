// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application_core.h"

#include <new>
#include <utility>

namespace granit::example::model_viewer {

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

granit::result application_core::prepare_upload(gltf::scene& scene,
                                                gltf_rendering::scene_plan& plan) const {
  if (phase_ != application_phase::gpu_upload)
    return granit::result::invalid_argument;
  try {
    scene = document_.scene();
    plan = gpu_plan_;
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  }
}

granit::result application_core::complete_upload(float recommended_exposure_ev,
                                                 float environment_intensity) noexcept {
  if (phase_ != application_phase::gpu_upload)
    return granit::result::invalid_argument;
  viewer_change recommended_lighting;
  recommended_lighting.environment_intensity = environment_intensity;
  recommended_lighting.exposure_ev = recommended_exposure_ev;
  if (document_.apply(recommended_lighting) != viewer_state_error::none)
    return granit::result::invalid_argument;
  phase_ = application_phase::ready;
  return granit::result::success;
}

granit::result application_core::tick(const viewer_document_update& input, viewer_frame& output) {
  if (phase_ != application_phase::ready)
    return granit::result::invalid_argument;
  viewer_document_frame document_frame;
  const auto document_result = document_.update(input, gpu_plan_, document_frame);
  if (document_result.failed())
    return document_result;
  viewer_frame candidate;
  candidate.view = document_frame.view;
  candidate.directional_light = document_frame.directional_light;
  candidate.width = input.width;
  candidate.height = input.height;
  candidate.exposure_ev = document_frame.exposure_ev;
  candidate.clear_color = document_frame.clear_color;
  candidate.environment_intensity = document_frame.environment_intensity;
  candidate.environment_rotation_radians = document_frame.environment_rotation_radians;
  output = std::move(candidate);
  return granit::result::success;
}

granit::result
application_core::update_material(std::uint32_t material_index,
                                  const gltf_rendering::material_factor_update& edit) noexcept {
  if (phase_ != application_phase::ready || material_index >= document_.scene().materials.size())
    return granit::result::invalid_argument;
  auto& material = document_.scene().materials[material_index];
  material.base_color = edit.base_color;
  material.metallic = edit.metallic;
  material.roughness = edit.roughness;
  material.normal_scale = edit.normal_scale;
  material.occlusion_strength = edit.occlusion_strength;
  material.emissive = edit.emissive;
  return granit::result::success;
}

void application_core::fail(granit::result result, std::string diagnostic) {
  failure_result_ = result;
  diagnostic_ = std::move(diagnostic);
  phase_ = application_phase::failed;
}

void application_core::reset() noexcept {
  document_.clear();
  gpu_plan_ = {};
  diagnostic_.clear();
  failure_result_ = granit::result::success;
  phase_ = application_phase::platform_ready;
}

} // namespace granit::example::model_viewer
