// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "gpu_scene.h"
#include "gltf_rendering/standard_pbr_assets.h"

#include <granit/pipeline/pbr_material.hpp>
#include <granit/renderer/upload_batch.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace granit::example::gltf_rendering {
namespace {

constexpr std::array<std::byte, 4> white_pixel{std::byte{255}, std::byte{255}, std::byte{255},
                                               std::byte{255}};
constexpr std::array<std::byte, 4> normal_pixel{std::byte{128}, std::byte{128}, std::byte{255},
                                                std::byte{255}};
constexpr std::uint32_t texture_upload_row_alignment = 256;

std::uint32_t full_mip_count(std::uint32_t width, std::uint32_t height) noexcept {
  std::uint32_t count = 1;
  for (auto extent = std::max(width, height); extent > 1; extent /= 2)
    ++count;
  return count;
}

float srgb_to_linear(std::uint8_t value) noexcept {
  const auto normalized = static_cast<float>(value) / 255.0F;
  return normalized <= 0.04045F ? normalized / 12.92F
                                : std::pow((normalized + 0.055F) / 1.055F, 2.4F);
}

std::uint8_t linear_to_srgb(float value) noexcept {
  const auto encoded =
      value <= 0.0031308F ? value * 12.92F : 1.055F * std::pow(value, 1.0F / 2.4F) - 0.055F;
  return static_cast<std::uint8_t>(std::lround(std::clamp(encoded, 0.0F, 1.0F) * 255.0F));
}

bool build_rgba8_mip_chain(const gltf::image& source, bool srgb, std::vector<std::byte>& pixels,
                           std::vector<gltf::image_mip>& mips) {
  if (source.mips.size() != 1)
    return false;
  const auto& base = source.mips.front();
  const auto base_size = std::uint64_t{base.width} * base.height * 4;
  if (base.width == 0 || base.height == 0 || base.offset > source.rgba8_pixels.size() ||
      base_size > std::numeric_limits<std::size_t>::max() ||
      base.size != static_cast<std::size_t>(base_size) ||
      base.size > source.rgba8_pixels.size() - base.offset)
    return false;

  const auto base_bytes = std::span{source.rgba8_pixels}.subspan(base.offset, base.size);
  pixels.assign(base_bytes.begin(), base_bytes.end());
  mips.push_back({base.width, base.height, 0, base.size});
  auto width = base.width;
  auto height = base.height;
  while (width > 1 || height > 1) {
    const auto next_width = std::max(UINT32_C(1), width / 2);
    const auto next_height = std::max(UINT32_C(1), height / 2);
    const auto offset = pixels.size();
    pixels.resize(offset + std::size_t{next_width} * next_height * 4);
    const auto previous_offset = mips.back().offset;
    for (std::uint32_t y = 0; y < next_height; ++y) {
      for (std::uint32_t x = 0; x < next_width; ++x) {
        const auto destination = offset + (std::size_t{y} * next_width + x) * 4;
        for (std::uint32_t channel = 0; channel < 4; ++channel) {
          float sum{};
          std::uint32_t count{};
          for (std::uint32_t dy = 0; dy < 2; ++dy) {
            const auto source_y = y * 2 + dy;
            if (source_y >= height)
              continue;
            for (std::uint32_t dx = 0; dx < 2; ++dx) {
              const auto source_x = x * 2 + dx;
              if (source_x >= width)
                continue;
              const auto source_index =
                  previous_offset + (std::size_t{source_y} * width + source_x) * 4 + channel;
              const auto value = std::to_integer<std::uint8_t>(pixels[source_index]);
              sum +=
                  srgb && channel < 3 ? srgb_to_linear(value) : static_cast<float>(value) / 255.0F;
              ++count;
            }
          }
          const auto average = sum / static_cast<float>(count);
          pixels[destination + channel] = static_cast<std::byte>(
              srgb && channel < 3 ? linear_to_srgb(average)
                                  : static_cast<std::uint8_t>(std::lround(average * 255.0F)));
        }
      }
    }
    const auto size = std::size_t{next_width} * next_height * 4;
    mips.push_back({next_width, next_height, offset, size});
    width = next_width;
    height = next_height;
  }
  return true;
}

granit::result create_default_texture(granit::renderer_ref renderer, granit::upload_batch& uploads,
                                      bool srgb, std::span<const std::byte, 4> pixel,
                                      gpu_texture& output) {
  const auto format =
      srgb ? granit::texture_format::rgba8_srgb : granit::texture_format::rgba8_unorm;
  if (const auto result =
          output.texture.initialize(renderer, {.format = format,
                                               .usage = granit::texture_usage::sampled |
                                                        granit::texture_usage::transfer_destination,
                                               .location = granit::memory_location::device});
      result.failed())
    return result;
  if (const auto result =
          output.view.initialize(renderer, output.texture.ref(), {.format = format});
      result.failed())
    return result;
  return uploads.write_texture(output.texture.ref(), pixel,
                               {.bytes_per_row = 4, .rows_per_image = 1}, {});
}

const gpu_texture* find_texture(const std::vector<gpu_texture>& textures, std::uint32_t image,
                                bool srgb) {
  const auto found = std::ranges::find_if(textures, [=](const gpu_texture& texture) {
    return texture.variant == texture_variant{image, srgb};
  });
  return found == textures.end() ? nullptr : &*found;
}

granit::texture_view_ref resolve_texture(const gltf::texture_reference& reference, bool srgb,
                                         const std::vector<gpu_texture>& textures,
                                         const gpu_texture& fallback) {
  if (reference.image != gltf::invalid_index) {
    if (const auto* texture = find_texture(textures, reference.image, srgb))
      return texture->view.ref();
  }
  return fallback.view.ref();
}

granit::result resolve_sampler(const gltf::texture_reference& reference, const gpu_scene_plan& plan,
                               const std::vector<granit::sampler>& samplers,
                               const granit::sampler& fallback, granit::sampler_ref& output) {
  if (reference.sampler == gltf::invalid_index) {
    output = fallback.ref();
    return granit::result::success;
  }
  if (reference.sampler >= plan.source_sampler_to_plan.size())
    return granit::result::invalid_argument;
  const auto selected = plan.source_sampler_to_plan[reference.sampler];
  if (selected >= samplers.size())
    return granit::result::invalid_argument;
  output = samplers[selected].ref();
  return granit::result::success;
}

granit::result create_material(granit::renderer_ref renderer, const gltf::material& source,
                               const gpu_scene_plan& plan, const std::vector<gpu_texture>& textures,
                               const std::vector<granit::sampler>& samplers,
                               const default_material_textures& defaults,
                               const granit::sampler& default_sampler,
                               granit::shader_library_ref shader_library,
                               granit::material_instance& output) {
  const auto base_color =
      resolve_texture(source.base_color_texture, true, textures, defaults.white_srgb);
  const auto metallic_roughness =
      resolve_texture(source.metallic_roughness_texture, false, textures, defaults.white_linear);
  const auto normal =
      resolve_texture(source.normal_texture, false, textures, defaults.normal_linear);
  const auto occlusion =
      resolve_texture(source.occlusion_texture, false, textures, defaults.white_linear);
  const auto emissive =
      resolve_texture(source.emissive_texture, true, textures, defaults.white_srgb);
  const std::array references{&source.base_color_texture, &source.metallic_roughness_texture,
                              &source.normal_texture, &source.occlusion_texture,
                              &source.emissive_texture};
  std::array<granit::sampler_ref, 5> resolved_samplers;
  std::uint32_t uv1_mask{};
  for (std::size_t index = 0; index < references.size(); ++index) {
    if (const auto result = resolve_sampler(*references[index], plan, samplers, default_sampler,
                                            resolved_samplers[index]);
        result.failed())
      return result;
    if (references[index]->image != gltf::invalid_index &&
        references[index]->texture_coordinate == 1)
      uv1_mask |= UINT32_C(1) << index;
  }
  const std::array updates{
      granit::material_parameter_update::value(granit::material_parameter_id("base_color"),
                                               granit::material_parameter_type::float4,
                                               std::as_bytes(std::span{&source.base_color, 1})),
      granit::material_parameter_update::value(granit::material_parameter_id("metallic"),
                                               granit::material_parameter_type::float32,
                                               std::as_bytes(std::span{&source.metallic, 1})),
      granit::material_parameter_update::value(
          granit::material_parameter_id("perceptual_roughness"),
          granit::material_parameter_type::float32, std::as_bytes(std::span{&source.roughness, 1})),
      granit::material_parameter_update::value(granit::material_parameter_id("normal_scale"),
                                               granit::material_parameter_type::float32,
                                               std::as_bytes(std::span{&source.normal_scale, 1})),
      granit::material_parameter_update::value(
          granit::material_parameter_id("occlusion_strength"),
          granit::material_parameter_type::float32,
          std::as_bytes(std::span{&source.occlusion_strength, 1})),
      granit::material_parameter_update::value(granit::material_parameter_id("emissive"),
                                               granit::material_parameter_type::float3,
                                               std::as_bytes(std::span{&source.emissive, 1})),
      granit::material_parameter_update::value(granit::material_parameter_id("alpha_cutoff"),
                                               granit::material_parameter_type::float32,
                                               std::as_bytes(std::span{&source.alpha_cutoff, 1})),
      granit::material_parameter_update::value(granit::material_parameter_id("uv1_mask"),
                                               granit::material_parameter_type::uint32,
                                               std::as_bytes(std::span{&uv1_mask, 1})),
      granit::material_parameter_update::texture_binding(
          granit::material_parameter_id("base_color_texture"), base_color),
      granit::material_parameter_update::texture_binding(
          granit::material_parameter_id("metallic_roughness_texture"), metallic_roughness),
      granit::material_parameter_update::texture_binding(
          granit::material_parameter_id("normal_texture"), normal),
      granit::material_parameter_update::texture_binding(
          granit::material_parameter_id("occlusion_texture"), occlusion),
      granit::material_parameter_update::texture_binding(
          granit::material_parameter_id("emissive_texture"), emissive),
      granit::material_parameter_update::sampler_binding(
          granit::material_parameter_id("base_color_sampler"), resolved_samplers[0]),
      granit::material_parameter_update::sampler_binding(
          granit::material_parameter_id("metallic_roughness_sampler"), resolved_samplers[1]),
      granit::material_parameter_update::sampler_binding(
          granit::material_parameter_id("normal_sampler"), resolved_samplers[2]),
      granit::material_parameter_update::sampler_binding(
          granit::material_parameter_id("occlusion_sampler"), resolved_samplers[3]),
      granit::material_parameter_update::sampler_binding(
          granit::material_parameter_id("emissive_sampler"), resolved_samplers[4]),
  };
  const auto archive = standard_pbr_material_archive();
  const granit::material_desc desc{
      .archive = archive,
      .initial_updates = updates,
      .shader_library = shader_library,
  };
  return output.initialize(renderer, desc);
}

} // namespace

granit::result
gpu_scene::add_pipeline_warmups(pipeline_warmup_batch_ref batch, texture_format color_format,
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

gpu_scene::gpu_scene(gpu_scene&& other) noexcept
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

gpu_scene& gpu_scene::operator=(gpu_scene&& other) noexcept {
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

granit::result gpu_scene::initialize(granit::renderer_ref renderer, const gltf::scene& source,
                                     float sampler_anisotropy, gpu_scene_upload_callback progress,
                                     void* progress_user_data) {
  gpu_scene_plan plan;
  const auto plan_result = build_gpu_scene_plan(source, plan);
  if (plan_result != gpu_scene_plan_error::none)
    return plan_result == gpu_scene_plan_error::out_of_memory ? granit::result::out_of_memory
                                                              : granit::result::invalid_argument;
  return initialize(renderer, source, std::move(plan), sampler_anisotropy, progress,
                    progress_user_data);
}

granit::result gpu_scene::initialize(granit::renderer_ref renderer, const gltf::scene& source,
                                     gpu_scene_plan plan, float sampler_anisotropy,
                                     gpu_scene_upload_callback progress, void* progress_user_data) {
  gpu_scene candidate;
  const auto result = candidate.create(renderer, source, std::move(plan), sampler_anisotropy,
                                       progress, progress_user_data);
  if (result.failed())
    return result;
  *this = std::move(candidate);
  return granit::result::success;
}

void gpu_scene::reset() noexcept {
  if (!valid())
    return;
  [[maybe_unused]] gpu_scene retired(std::move(*this));
}

granit::result gpu_scene::texture_binding(const gltf::texture_reference& reference, bool srgb,
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
gpu_scene::create_snapshot(std::span<const granit_scene_view> views,
                           std::span<const granit_scene_directional_light> directional_lights,
                           std::span<const granit_scene_point_light> point_lights,
                           std::span<const granit_scene_spot_light> spot_lights,
                           granit::scene_snapshot& output) const noexcept {
  if (!valid())
    return granit::result::invalid_handle;
  if (views.size() > std::numeric_limits<std::uint32_t>::max() ||
      plan_.renderables.size() > std::numeric_limits<std::uint32_t>::max() ||
      directional_lights.size() > std::numeric_limits<std::uint32_t>::max() ||
      point_lights.size() > std::numeric_limits<std::uint32_t>::max() ||
      spot_lights.size() > std::numeric_limits<std::uint32_t>::max())
    return granit::result::invalid_argument;
  const granit::scene_snapshot_desc desc{
      .views = views,
      .renderables = plan_.renderables,
      .directional_lights = directional_lights,
      .point_lights = point_lights,
      .spot_lights = spot_lights,
  };
  return output.initialize(renderer_, desc);
}

granit::result gpu_scene::update_material_factors(gltf::scene& source, std::uint32_t material_index,
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

granit::result gpu_scene::update_debug_display(std::uint32_t mode) noexcept {
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

granit::result gpu_scene::create(granit::renderer_ref renderer, const gltf::scene& source,
                                 gpu_scene_plan plan, float sampler_anisotropy,
                                 gpu_scene_upload_callback progress, void* progress_user_data) {
  const auto report = [&](gpu_scene_upload_stage stage, std::size_t completed, std::size_t total) {
    if (progress == nullptr)
      return true;
    if (completed > std::numeric_limits<std::uint32_t>::max() ||
        total > std::numeric_limits<std::uint32_t>::max())
      return false;
    return progress(
        {stage, static_cast<std::uint32_t>(completed), static_cast<std::uint32_t>(total)},
        progress_user_data);
  };
  if (!renderer.valid())
    return granit::result::invalid_handle;
  if (!std::isfinite(sampler_anisotropy) || sampler_anisotropy < 1.0F)
    return granit::result::invalid_argument;
  plan_ = std::move(plan);
  if (!report(gpu_scene_upload_stage::planning, 1, 1))
    return granit::result::cancelled;

  granit::upload_batch uploads;
  if (const auto result = uploads.initialize(renderer); result.failed())
    return result;
  std::vector<granit::async_operation> upload_operations;
  const auto submit_uploads = [&]() -> granit::result {
    granit::async_operation operation;
    if (const auto result = uploads.submit_async(operation); result.failed())
      return result;
    upload_operations.push_back(std::move(operation));
    return granit::result::success;
  };
  if (!plan_.vertices.empty()) {
    const auto size = plan_.vertices.size() * sizeof(packed_vertex);
    if (const auto result = vertex_buffer_.initialize(
            renderer,
            {.size = size,
             .usage = granit::buffer_usage::vertex | granit::buffer_usage::transfer_destination,
             .location = granit::memory_location::device});
        result.failed())
      return result;
    if (const auto result =
            uploads.write_buffer(vertex_buffer_.ref(), 0, std::as_bytes(std::span{plan_.vertices}));
        result.failed())
      return result;
  }
  if (!plan_.indices.empty()) {
    const auto size = plan_.indices.size() * sizeof(std::uint32_t);
    if (const auto result =
            index_buffer_.initialize(renderer, {.size = size,
                                                .usage = granit::buffer_usage::index |
                                                         granit::buffer_usage::transfer_destination,
                                                .location = granit::memory_location::device});
        result.failed())
      return result;
    if (const auto result =
            uploads.write_buffer(index_buffer_.ref(), 0, std::as_bytes(std::span{plan_.indices}));
        result.failed())
      return result;
  }
  if (!report(gpu_scene_upload_stage::geometry, 1, 1))
    return granit::result::cancelled;

  textures_.reserve(plan_.textures.size());
  std::size_t textures_in_batch = 0;
  for (std::size_t texture_index = 0; texture_index < plan_.textures.size(); ++texture_index) {
    const auto variant = plan_.textures[texture_index];
    const auto& source_image = source.images[variant.image];
    if (source_image.mips.empty())
      return granit::result::invalid_argument;
    gpu_texture target;
    target.variant = variant;
    const auto& base_mip = source_image.mips.front();
    std::vector<std::byte> generated_pixels;
    std::vector<gltf::image_mip> generated_mips;
    const auto generate_mips =
        source_image.mips.size() == 1 && full_mip_count(base_mip.width, base_mip.height) > 1;
    if (generate_mips &&
        !build_rgba8_mip_chain(source_image, variant.srgb, generated_pixels, generated_mips))
      return granit::result::invalid_argument;
    const auto& pixels = generate_mips ? generated_pixels : source_image.rgba8_pixels;
    const auto& mips = generate_mips ? generated_mips : source_image.mips;
    const auto mip_levels = static_cast<std::uint32_t>(mips.size());
    if (const auto result = target.texture.initialize(
            renderer,
            {.format = variant.srgb ? granit::texture_format::rgba8_srgb
                                    : granit::texture_format::rgba8_unorm,
             .usage = granit::texture_usage::sampled | granit::texture_usage::transfer_destination,
             .location = granit::memory_location::device,
             .width = base_mip.width,
             .height = base_mip.height,
             .mip_levels = mip_levels});
        result.failed())
      return result;
    if (const auto result =
            target.view.initialize(renderer, target.texture.ref(),
                                   {.format = variant.srgb ? granit::texture_format::rgba8_srgb
                                                           : granit::texture_format::rgba8_unorm,
                                    .mip_level_count = mip_levels});
        result.failed())
      return result;
    for (std::uint32_t mip_index = 0; mip_index < mips.size(); ++mip_index) {
      const auto& mip = mips[mip_index];
      if (mip.width > std::numeric_limits<std::uint32_t>::max() / 4 || mip.offset > pixels.size() ||
          mip.size > pixels.size() - mip.offset)
        return granit::result::invalid_argument;
      const auto bytes = std::span{pixels}.subspan(mip.offset, mip.size);
      const auto tight_row = mip.width * 4;
      const auto aligned_row = (std::uint64_t{tight_row} + texture_upload_row_alignment - 1) &
                               ~std::uint64_t{texture_upload_row_alignment - 1};
      if (aligned_row > std::numeric_limits<std::uint32_t>::max())
        return granit::result::invalid_argument;
      const auto row_pitch = static_cast<std::uint32_t>(aligned_row);
      std::vector<std::byte> padded_bytes;
      auto upload_bytes = bytes;
      if (row_pitch != tight_row && mip.height > 1) {
        padded_bytes.resize(std::size_t{row_pitch} * (mip.height - 1) + tight_row);
        for (std::uint32_t row = 0; row < mip.height; ++row) {
          std::memcpy(padded_bytes.data() + std::size_t{row} * row_pitch,
                      bytes.data() + std::size_t{row} * tight_row, tight_row);
        }
        upload_bytes = padded_bytes;
      }
      if (const auto result = uploads.write_texture(
              target.texture.ref(), upload_bytes,
              {.bytes_per_row = row_pitch, .rows_per_image = mip.height},
              {.mip_level = mip_index, .width = mip.width, .height = mip.height});
          result.failed())
        return result;
    }
    textures_.push_back(std::move(target));
    ++textures_in_batch;
    if (textures_in_batch == 2) {
      if (const auto result = submit_uploads(); result.failed())
        return result;
      textures_in_batch = 0;
    }
    if (!report(gpu_scene_upload_stage::textures, texture_index + 1, plan_.textures.size()))
      return granit::result::cancelled;
  }
  if (plan_.textures.empty() && !report(gpu_scene_upload_stage::textures, 0, 0))
    return granit::result::cancelled;

  samplers_.reserve(plan_.samplers.size());
  for (std::size_t sampler_index = 0; sampler_index < plan_.samplers.size(); ++sampler_index) {
    const auto& key = plan_.samplers[sampler_index];
    samplers_.emplace_back();
    const bool use_anisotropy = key.mag_filter == granit::filter::linear &&
                                key.min_filter == granit::filter::linear &&
                                key.mip_filter == granit::mipmap_filter::linear;
    const granit::sampler_desc desc{.mag_filter = key.mag_filter,
                                    .min_filter = key.min_filter,
                                    .mip_filter = key.mip_filter,
                                    .address_u = key.address_u,
                                    .address_v = key.address_v,
                                    .address_w = granit::address_mode::repeat,
                                    .anisotropy_enabled =
                                        use_anisotropy && sampler_anisotropy > 1.0F,
                                    .max_anisotropy = use_anisotropy ? sampler_anisotropy : 1.0F,
                                    .max_lod = 1000.0F};
    auto result = samplers_.back().initialize(renderer, desc);
    // 各向异性是画质增强项；设备限制较低时保留三线性采样，不阻止场景加载。
    if (result == granit::result::unsupported && use_anisotropy) {
      auto fallback = desc;
      fallback.anisotropy_enabled = false;
      fallback.max_anisotropy = 1.0F;
      result = samplers_.back().initialize(renderer, fallback);
    }
    if (result.failed())
      return result;
    if (!report(gpu_scene_upload_stage::samplers, sampler_index + 1, plan_.samplers.size()))
      return granit::result::cancelled;
  }
  if (plan_.samplers.empty() && !report(gpu_scene_upload_stage::samplers, 0, 0))
    return granit::result::cancelled;

  if (const auto result = default_sampler_.initialize(renderer, {.max_lod = 1000.0F});
      result.failed())
    return result;
  if (const auto result = create_default_texture(renderer, uploads, true, white_pixel,
                                                 default_textures_.white_srgb);
      result.failed())
    return result;
  if (const auto result = create_default_texture(renderer, uploads, false, white_pixel,
                                                 default_textures_.white_linear);
      result.failed())
    return result;
  if (const auto result = create_default_texture(renderer, uploads, false, normal_pixel,
                                                 default_textures_.normal_linear);
      result.failed())
    return result;

  constexpr std::array attributes{
      granit::vertex_attribute{0, granit::vertex_format::float32x3,
                               static_cast<std::uint32_t>(offsetof(packed_vertex, position)), 0},
      granit::vertex_attribute{1, granit::vertex_format::float32x3,
                               static_cast<std::uint32_t>(offsetof(packed_vertex, normal)), 0},
      granit::vertex_attribute{2, granit::vertex_format::float32x4,
                               static_cast<std::uint32_t>(offsetof(packed_vertex, tangent)), 0},
      granit::vertex_attribute{
          3, granit::vertex_format::float32x2,
          static_cast<std::uint32_t>(offsetof(packed_vertex, texture_coordinate)), 0},
      granit::vertex_attribute{
          4, granit::vertex_format::float32x2,
          static_cast<std::uint32_t>(offsetof(packed_vertex, texture_coordinate_1)), 0},
      granit::vertex_attribute{5, granit::vertex_format::float32x4,
                               static_cast<std::uint32_t>(offsetof(packed_vertex, color)), 0},
  };
  const granit::vertex_buffer_layout layout{.stride = sizeof(packed_vertex),
                                            .attributes = attributes};
  meshes_.reserve(plan_.primitives.size());
  for (std::size_t primitive_index = 0; primitive_index < plan_.primitives.size();
       ++primitive_index) {
    const auto& primitive = plan_.primitives[primitive_index];
    const granit::mesh_vertex_buffer binding{
        .buffer = vertex_buffer_.ref(), .offset = primitive.vertex_offset, .layout = layout};
    const granit::mesh_desc desc{.vertex_buffers = std::span{&binding, 1},
                                 .index_buffer = index_buffer_.ref(),
                                 .index_buffer_offset = primitive.index_offset,
                                 .index_format = granit::index_type::uint32,
                                 .vertex_count = primitive.vertex_count,
                                 .index_count = primitive.index_count};
    meshes_.emplace_back();
    if (const auto result = meshes_.back().initialize(renderer, desc); result.failed())
      return result;
    if (!report(gpu_scene_upload_stage::meshes, primitive_index + 1, plan_.primitives.size()))
      return granit::result::cancelled;
  }
  if (plan_.primitives.empty() && !report(gpu_scene_upload_stage::meshes, 0, 0))
    return granit::result::cancelled;
  if (const auto result = submit_uploads(); result.failed())
    return result;
  if (const auto result = shader_library_.initialize(renderer, standard_pbr_shader_library());
      result.failed())
    return result;
  materials_.reserve(source.materials.size() + 1);
  material_alpha_modes_.reserve(source.materials.size() + 1);
  for (std::size_t material_index = 0; material_index < source.materials.size(); ++material_index) {
    const auto& source_material = source.materials[material_index];
    materials_.emplace_back();
    if (const auto result = create_material(renderer, source_material, plan_, textures_, samplers_,
                                            default_textures_, default_sampler_,
                                            shader_library_.ref(), materials_.back());
        result.failed())
      return result;
    material_alpha_modes_.push_back(source_material.alpha_mode);
    if (!report(gpu_scene_upload_stage::materials, material_index + 1, source.materials.size() + 1))
      return granit::result::cancelled;
  }
  materials_.emplace_back();
  if (const auto result =
          create_material(renderer, {}, plan_, textures_, samplers_, default_textures_,
                          default_sampler_, shader_library_.ref(), materials_.back());
      result.failed())
    return result;
  material_alpha_modes_.push_back(gltf::material_alpha_mode::opaque);
  if (!report(gpu_scene_upload_stage::materials, source.materials.size() + 1,
              source.materials.size() + 1))
    return granit::result::cancelled;

  draw_bindings_.reserve(plan_.draws.size());
  for (const auto& draw : plan_.draws) {
    const auto material_index =
        draw.material == gltf::invalid_index ? source.materials.size() : draw.material;
    const auto* source_material =
        material_index < source.materials.size() ? &source.materials[material_index] : nullptr;
    const auto alpha_mode =
        source_material != nullptr && source_material->alpha_mode == gltf::material_alpha_mode::mask
            ? granit::pbr_alpha_mode::mask
        : source_material != nullptr &&
                source_material->alpha_mode == gltf::material_alpha_mode::blend
            ? granit::pbr_alpha_mode::blend
            : granit::pbr_alpha_mode::opaque;
    draw_bindings_.push_back(
        {.payload = draw.payload,
         .mesh = meshes_[draw.primitive].ref(),
         .material = materials_[material_index].ref(),
         .variant = granit::pbr_material_variant_key(
             granit::pbr_texture::all, alpha_mode,
             source_material != nullptr && source_material->double_sided, true, true)});
  }
  renderer_ = renderer;
  return granit::result::success;
}

} // namespace granit::example::gltf_rendering
