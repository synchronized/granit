// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "scene_resources.h"

#include <granit/pipeline/pbr_material.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>

namespace granit::example::gltf_rendering {
namespace {

const gpu_texture* find_texture(const std::vector<gpu_texture>& textures, std::uint32_t image,
                                bool srgb) {
  const auto found = std::ranges::find_if(textures, [=](const gpu_texture& texture) {
    return texture.variant == texture_variant{image, srgb};
  });
  return found == textures.end() ? nullptr : &*found;
}

} // namespace

granit::result
scene_resources::add_pipeline_warmups(pipeline_warmup_batch_ref batch, texture_format color_format,
                                      sample_count samples,
                                      std::vector<std::uint32_t>& result_indices) noexcept {
  result_indices.clear();
  if (!valid() || !batch || color_format == texture_format::undefined)
    return granit::result::invalid_argument;
  try {
    result_indices.reserve(materials_.size());
    for (std::size_t material_index = 0; material_index < materials_.size(); ++material_index) {
      const auto& material = materials_[material_index];
      const auto transparent =
          material_index < material_alpha_modes_.size() &&
          material_alpha_modes_[material_index] == gltf::material_alpha_mode::blend;
      const material_pipeline_warmup_desc desc{
          .pass = granit::material_parameter_id(transparent ? "transparent" : "opaque"),
          .color_format = color_format,
          .depth_stencil_format = texture_format::d32_float,
          .samples = samples,
      };
      std::uint32_t index{};
      const auto result = material.add_pipeline_warmup(desc, batch, index);
      if (result.failed()) {
        result_indices.clear();
        return result;
      }
      result_indices.push_back(index);
    }
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    result_indices.clear();
    return granit::result::out_of_memory;
  }
}

scene_resources::scene_resources(scene_resources&& other) noexcept
    : renderer_(other.renderer_), plan_(std::move(other.plan_)),
      vertex_buffer_(std::move(other.vertex_buffer_)),
      index_buffer_(std::move(other.index_buffer_)), textures_(std::move(other.textures_)),
      samplers_(std::move(other.samplers_)), meshes_(std::move(other.meshes_)),
      default_textures_(std::move(other.default_textures_)),
      default_sampler_(std::move(other.default_sampler_)),
      shader_library_(std::move(other.shader_library_)), materials_(std::move(other.materials_)),
      material_alpha_modes_(std::move(other.material_alpha_modes_)),
      draw_bindings_(std::move(other.draw_bindings_)) {
  other.renderer_ = {};
}

scene_resources& scene_resources::operator=(scene_resources&& other) noexcept {
  if (this != &other) {
    reset();
    renderer_ = other.renderer_;
    other.renderer_ = {};
    plan_ = std::move(other.plan_);
    vertex_buffer_ = std::move(other.vertex_buffer_);
    index_buffer_ = std::move(other.index_buffer_);
    textures_ = std::move(other.textures_);
    samplers_ = std::move(other.samplers_);
    meshes_ = std::move(other.meshes_);
    default_textures_ = std::move(other.default_textures_);
    default_sampler_ = std::move(other.default_sampler_);
    shader_library_ = std::move(other.shader_library_);
    materials_ = std::move(other.materials_);
    material_alpha_modes_ = std::move(other.material_alpha_modes_);
    draw_bindings_ = std::move(other.draw_bindings_);
  }
  return *this;
}

void scene_resources::reset() noexcept {
  if (!valid())
    return;
  [[maybe_unused]] scene_resources retired(std::move(*this));
}

granit::result scene_resources::texture_binding(const gltf::texture_reference& reference, bool srgb,
                                                granit::texture_view_ref& view,
                                                granit::sampler_ref& sampler) const noexcept {
  view = {};
  sampler = {};
  if (!valid())
    return granit::result::invalid_handle;
  const auto* texture = find_texture(textures_, reference.image, srgb);
  if (texture == nullptr)
    return granit::result::invalid_argument;
  const granit::sampler* selected_sampler = &default_sampler_;
  if (reference.sampler != gltf::invalid_index) {
    if (reference.sampler >= plan_.source_sampler_to_plan.size())
      return granit::result::invalid_argument;
    const auto mapped = plan_.source_sampler_to_plan[reference.sampler];
    if (mapped >= samplers_.size())
      return granit::result::invalid_argument;
    selected_sampler = &samplers_[mapped];
  }
  view = texture->view.ref();
  sampler = selected_sampler->ref();
  return granit::result::success;
}

granit::result
scene_resources::update_material_factors(gltf::scene& source, std::uint32_t material_index,
                                         const material_factor_update& edit) noexcept {
  const auto finite = [](float value) { return std::isfinite(value); };
  const auto unit = [&](float value) { return finite(value) && value >= 0.0F && value <= 1.0F; };
  if (!valid() || material_index >= source.materials.size() || material_index >= materials_.size())
    return valid() ? granit::result::invalid_argument : granit::result::invalid_handle;
  if (!unit(edit.base_color.x) || !unit(edit.base_color.y) || !unit(edit.base_color.z) ||
      !unit(edit.base_color.w) || !unit(edit.metallic) || !unit(edit.roughness) ||
      !finite(edit.normal_scale) || edit.normal_scale < 0.0F || edit.normal_scale > 10.0F ||
      !unit(edit.occlusion_strength) || !finite(edit.emissive.x) || !finite(edit.emissive.y) ||
      !finite(edit.emissive.z) || edit.emissive.x < 0.0F || edit.emissive.y < 0.0F ||
      edit.emissive.z < 0.0F)
    return granit::result::invalid_argument;

  const std::array updates{
      granit::material_parameter_update::value(granit::material_parameter_id("base_color"),
                                               granit::material_parameter_type::float4,
                                               std::as_bytes(std::span{&edit.base_color, 1})),
      granit::material_parameter_update::value(granit::material_parameter_id("metallic"),
                                               granit::material_parameter_type::float32,
                                               std::as_bytes(std::span{&edit.metallic, 1})),
      granit::material_parameter_update::value(
          granit::material_parameter_id("perceptual_roughness"),
          granit::material_parameter_type::float32, std::as_bytes(std::span{&edit.roughness, 1})),
      granit::material_parameter_update::value(granit::material_parameter_id("normal_scale"),
                                               granit::material_parameter_type::float32,
                                               std::as_bytes(std::span{&edit.normal_scale, 1})),
      granit::material_parameter_update::value(
          granit::material_parameter_id("occlusion_strength"),
          granit::material_parameter_type::float32,
          std::as_bytes(std::span{&edit.occlusion_strength, 1})),
      granit::material_parameter_update::value(granit::material_parameter_id("emissive"),
                                               granit::material_parameter_type::float3,
                                               std::as_bytes(std::span{&edit.emissive, 1})),
  };
  const auto result = materials_[material_index].update(updates);
  if (result.failed())
    return result;
  auto& material = source.materials[material_index];
  material.base_color = edit.base_color;
  material.metallic = edit.metallic;
  material.roughness = edit.roughness;
  material.normal_scale = edit.normal_scale;
  material.occlusion_strength = edit.occlusion_strength;
  material.emissive = edit.emissive;
  return granit::result::success;
}

granit::result scene_resources::update_debug_display(std::uint32_t mode) noexcept {
  if (!valid())
    return granit::result::invalid_handle;
  if (mode > static_cast<std::uint32_t>(debug_display_mode::vertex_tangents))
    return granit::result::invalid_argument;
  const auto update = granit::material_parameter_update::value(
      granit::material_parameter_id("debug_display"), granit::material_parameter_type::uint32,
      std::as_bytes(std::span{&mode, 1}));
  for (auto& material : materials_) {
    if (const auto result = material.update(std::span{&update, 1}); result.failed())
      return result;
  }
  return granit::result::success;
}

} // namespace granit::example::gltf_rendering
