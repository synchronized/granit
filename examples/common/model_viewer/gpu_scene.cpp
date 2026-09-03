// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/gpu_scene.h"
#include "model_viewer/material_archive.h"
#include "model_viewer/viewer_state.h"

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

namespace granit::example::model_viewer {
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
  const auto encoded = value <= 0.0031308F ? value * 12.92F
                                           : 1.055F * std::pow(value, 1.0F / 2.4F) - 0.055F;
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
              sum += srgb && channel < 3 ? srgb_to_linear(value)
                                         : static_cast<float>(value) / 255.0F;
              ++count;
            }
          }
          const auto average = sum / static_cast<float>(count);
          pixels[destination + channel] =
              static_cast<std::byte>(srgb && channel < 3
                                         ? linear_to_srgb(average)
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

granit::result create_default_texture(granit_renderer renderer, granit::upload_batch& uploads,
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
          output.view.initialize(renderer, output.texture.native_handle(), {.format = format});
      result.failed())
    return result;
  return uploads.write_texture(output.texture.native_handle(), pixel,
                               {.bytes_per_row = 4, .rows_per_image = 1}, {});
}

const gpu_texture* find_texture(const std::vector<gpu_texture>& textures, std::uint32_t image,
                                bool srgb) {
  const auto found = std::ranges::find_if(textures, [=](const gpu_texture& texture) {
    return texture.variant == texture_variant{image, srgb};
  });
  return found == textures.end() ? nullptr : &*found;
}

granit_texture_view resolve_texture(const gltf::texture_reference& reference, bool srgb,
                                    const std::vector<gpu_texture>& textures,
                                    const gpu_texture& fallback) {
  if (reference.image != gltf::invalid_index) {
    if (const auto* texture = find_texture(textures, reference.image, srgb))
      return texture->view.native_handle();
  }
  return fallback.view.native_handle();
}

granit::result resolve_material_sampler(const gltf::material& material, const gpu_scene_plan& plan,
                                        const std::vector<granit::sampler>& samplers,
                                        const granit::sampler& fallback, granit_sampler& output) {
  const std::array references{&material.base_color_texture, &material.metallic_roughness_texture,
                              &material.normal_texture, &material.occlusion_texture,
                              &material.emissive_texture};
  std::uint32_t selected = gltf::invalid_index;
  for (const auto* reference : references) {
    if (reference->image == gltf::invalid_index || reference->sampler == gltf::invalid_index)
      continue;
    if (reference->sampler >= plan.source_sampler_to_plan.size())
      return granit::result::invalid_argument;
    const auto candidate = plan.source_sampler_to_plan[reference->sampler];
    if (selected != gltf::invalid_index && candidate != selected)
      return granit::result::unsupported;
    selected = candidate;
  }
  if (selected != gltf::invalid_index) {
    if (selected >= samplers.size())
      return granit::result::invalid_argument;
    output = samplers[selected].native_handle();
  } else {
    output = fallback.native_handle();
  }
  return granit::result::success;
}

granit::result create_material(granit_renderer renderer, const gltf::material& source,
                               const gpu_scene_plan& plan, const std::vector<gpu_texture>& textures,
                               const std::vector<granit::sampler>& samplers,
                               const default_material_textures& defaults,
                               const granit::sampler& default_sampler,
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
  granit_sampler sampler = GRANIT_NULL_HANDLE;
  if (const auto result =
          resolve_material_sampler(source, plan, samplers, default_sampler, sampler);
      result.failed())
    return result;
  const std::array updates{
      granit_material_parameter_update{granit::material_parameter_id("base_color"),
                                       GRANIT_MATERIAL_PARAMETER_FLOAT4, 0, &source.base_color,
                                       sizeof(source.base_color), 0},
      granit_material_parameter_update{granit::material_parameter_id("metallic"),
                                       GRANIT_MATERIAL_PARAMETER_FLOAT32, 0, &source.metallic,
                                       sizeof(source.metallic), 0},
      granit_material_parameter_update{granit::material_parameter_id("perceptual_roughness"),
                                       GRANIT_MATERIAL_PARAMETER_FLOAT32, 0, &source.roughness,
                                       sizeof(source.roughness), 0},
      granit_material_parameter_update{granit::material_parameter_id("normal_scale"),
                                       GRANIT_MATERIAL_PARAMETER_FLOAT32, 0, &source.normal_scale,
                                       sizeof(source.normal_scale), 0},
      granit_material_parameter_update{
          granit::material_parameter_id("occlusion_strength"), GRANIT_MATERIAL_PARAMETER_FLOAT32, 0,
          &source.occlusion_strength, sizeof(source.occlusion_strength), 0},
      granit_material_parameter_update{granit::material_parameter_id("emissive"),
                                       GRANIT_MATERIAL_PARAMETER_FLOAT3, 0, &source.emissive,
                                       sizeof(source.emissive), 0},
      granit_material_parameter_update{granit::material_parameter_id("base_color_texture"),
                                       GRANIT_MATERIAL_PARAMETER_TEXTURE_VIEW, 0, nullptr, 0,
                                       base_color},
      granit_material_parameter_update{granit::material_parameter_id("metallic_roughness_texture"),
                                       GRANIT_MATERIAL_PARAMETER_TEXTURE_VIEW, 0, nullptr, 0,
                                       metallic_roughness},
      granit_material_parameter_update{granit::material_parameter_id("normal_texture"),
                                       GRANIT_MATERIAL_PARAMETER_TEXTURE_VIEW, 0, nullptr, 0,
                                       normal},
      granit_material_parameter_update{granit::material_parameter_id("occlusion_texture"),
                                       GRANIT_MATERIAL_PARAMETER_TEXTURE_VIEW, 0, nullptr, 0,
                                       occlusion},
      granit_material_parameter_update{granit::material_parameter_id("emissive_texture"),
                                       GRANIT_MATERIAL_PARAMETER_TEXTURE_VIEW, 0, nullptr, 0,
                                       emissive},
      granit_material_parameter_update{granit::material_parameter_id("pbr_sampler"),
                                       GRANIT_MATERIAL_PARAMETER_SAMPLER, 0, nullptr, 0, sampler},
  };
  const auto archive = model_viewer_material_archive();
  granit_material_desc desc = GRANIT_MATERIAL_DESC_INIT;
  desc.archive_data = archive.data();
  desc.archive_size = archive.size();
  desc.initial_updates = updates.data();
  desc.initial_update_count = static_cast<std::uint32_t>(updates.size());
  return output.initialize(renderer, desc);
}

math::float3 transform_point(const math::matrix4& matrix, const math::float3& point) {
  return {matrix[0] * point.x + matrix[4] * point.y + matrix[8] * point.z + matrix[12],
          matrix[1] * point.x + matrix[5] * point.y + matrix[9] * point.z + matrix[13],
          matrix[2] * point.x + matrix[6] * point.y + matrix[10] * point.z + matrix[14]};
}

float maximum_axis_scale(const math::matrix4& matrix) {
  float maximum = 0.0F;
  for (std::size_t column = 0; column < 3; ++column) {
    const auto offset = column * 4;
    const auto length =
        std::sqrt(matrix[offset] * matrix[offset] + matrix[offset + 1] * matrix[offset + 1] +
                  matrix[offset + 2] * matrix[offset + 2]);
    maximum = std::max(maximum, length);
  }
  return maximum;
}

math::matrix4 make_normal_matrix(const math::matrix4& matrix) {
  const auto a = matrix[0];
  const auto b = matrix[4];
  const auto c = matrix[8];
  const auto d = matrix[1];
  const auto e = matrix[5];
  const auto f = matrix[9];
  const auto g = matrix[2];
  const auto h = matrix[6];
  const auto i = matrix[10];
  const auto determinant = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
  if (!std::isfinite(determinant) || std::abs(determinant) <= 1.0e-8F)
    return math::identity_matrix4;
  const auto inverse = 1.0F / determinant;
  return {{(e * i - f * h) * inverse, (b * i - c * h) * -inverse, (b * f - c * e) * inverse, 0,
           (d * i - f * g) * -inverse, (a * i - c * g) * inverse, (a * f - c * d) * -inverse, 0,
           (d * h - e * g) * inverse, (a * h - b * g) * -inverse, (a * e - b * d) * inverse, 0, 0,
           0, 0, 1}};
}

void append_draw_bounds(const gltf::primitive& primitive, const gltf::node& node,
                        packed_draw& draw) {
  draw.model = node.world_transform;
  draw.normal_matrix = make_normal_matrix(node.world_transform);
  if (!primitive.local_bounds.valid)
    return;
  const math::float3 center{
      (primitive.local_bounds.minimum.x + primitive.local_bounds.maximum.x) * 0.5F,
      (primitive.local_bounds.minimum.y + primitive.local_bounds.maximum.y) * 0.5F,
      (primitive.local_bounds.minimum.z + primitive.local_bounds.maximum.z) * 0.5F};
  const math::float3 extent{primitive.local_bounds.maximum.x - center.x,
                            primitive.local_bounds.maximum.y - center.y,
                            primitive.local_bounds.maximum.z - center.z};
  draw.bounds_center = transform_point(node.world_transform, center);
  draw.bounds_radius = std::sqrt(extent.x * extent.x + extent.y * extent.y + extent.z * extent.z) *
                       maximum_axis_scale(node.world_transform);
}

void append_texture(std::vector<texture_variant>& output, const gltf::texture_reference& reference,
                    bool srgb) {
  if (reference.image == gltf::invalid_index)
    return;
  const texture_variant variant{.image = reference.image, .srgb = srgb};
  if (std::ranges::find(output, variant) == output.end())
    output.push_back(variant);
}

granit::address_mode normalize_wrap(std::uint32_t value) {
  if (value == 33071)
    return granit::address_mode::clamp_to_edge;
  if (value == 33648)
    return granit::address_mode::mirrored_repeat;
  return granit::address_mode::repeat;
}

sampler_key normalize_sampler(const gltf::sampler& source) {
  const bool nearest_min =
      source.min_filter == 9728 || source.min_filter == 9984 || source.min_filter == 9986;
  const bool nearest_mip = source.min_filter == 9728 || source.min_filter == 9729 ||
                           source.min_filter == 9984 || source.min_filter == 9985;
  return {
      .mag_filter = source.mag_filter == 9728 ? granit::filter::nearest : granit::filter::linear,
      .min_filter = nearest_min ? granit::filter::nearest : granit::filter::linear,
      .mip_filter = nearest_mip ? granit::mipmap_filter::nearest : granit::mipmap_filter::linear,
      .address_u = normalize_wrap(source.wrap_u),
      .address_v = normalize_wrap(source.wrap_v),
  };
}

gpu_scene_plan_error append_primitive(const gltf::primitive& source, gpu_scene_plan& output) {
  const auto vertex_count = source.positions.size();
  if (vertex_count != source.normals.size() ||
      (!source.tangents.empty() && source.tangents.size() != vertex_count) ||
      (!source.texture_coordinates.empty() && source.texture_coordinates.size() != vertex_count))
    return gpu_scene_plan_error::invalid_scene;
  if (vertex_count > std::numeric_limits<std::uint32_t>::max() ||
      source.indices.size() > std::numeric_limits<std::uint32_t>::max())
    return gpu_scene_plan_error::numeric_overflow;
  if (vertex_count > output.vertices.max_size() - output.vertices.size() ||
      source.indices.size() > output.indices.max_size() - output.indices.size() ||
      output.primitives.size() == output.primitives.max_size())
    return gpu_scene_plan_error::numeric_overflow;
  if constexpr (sizeof(std::size_t) > sizeof(std::uint64_t)) {
    if (output.vertices.size() >
            std::numeric_limits<std::uint64_t>::max() / sizeof(packed_vertex) ||
        output.indices.size() > std::numeric_limits<std::uint64_t>::max() / sizeof(std::uint32_t))
      return gpu_scene_plan_error::numeric_overflow;
  }

  packed_primitive primitive{.vertex_offset = output.vertices.size() * sizeof(packed_vertex),
                             .index_offset = output.indices.size() * sizeof(std::uint32_t),
                             .vertex_count = static_cast<std::uint32_t>(vertex_count),
                             .index_count = static_cast<std::uint32_t>(source.indices.size()),
                             .material = source.material};
  output.vertices.reserve(output.vertices.size() + vertex_count);
  for (std::size_t index = 0; index < vertex_count; ++index) {
    output.vertices.push_back({
        .position = source.positions[index],
        .normal = source.normals[index],
        .tangent = source.tangents.empty() ? math::float4{1, 0, 0, 1} : source.tangents[index],
        .texture_coordinate =
            source.texture_coordinates.empty() ? math::float2{} : source.texture_coordinates[index],
    });
  }
  output.indices.insert(output.indices.end(), source.indices.begin(), source.indices.end());
  output.primitives.push_back(primitive);
  return gpu_scene_plan_error::none;
}

} // namespace

gpu_scene_plan_error build_gpu_scene_plan(const gltf::scene& source, gpu_scene_plan& output) {
  try {
    gpu_scene_plan candidate;
    std::vector<std::uint32_t> mesh_primitive_starts;
    mesh_primitive_starts.reserve(source.meshes.size() + 1);
    for (const auto& mesh : source.meshes) {
      if (candidate.primitives.size() > std::numeric_limits<std::uint32_t>::max())
        return gpu_scene_plan_error::numeric_overflow;
      mesh_primitive_starts.push_back(static_cast<std::uint32_t>(candidate.primitives.size()));
      for (const auto& primitive : mesh.primitives) {
        if (primitive.material != gltf::invalid_index &&
            primitive.material >= source.materials.size())
          return gpu_scene_plan_error::invalid_scene;
        if (const auto result = append_primitive(primitive, candidate);
            result != gpu_scene_plan_error::none)
          return result;
      }
    }
    if (candidate.primitives.size() > std::numeric_limits<std::uint32_t>::max())
      return gpu_scene_plan_error::numeric_overflow;
    mesh_primitive_starts.push_back(static_cast<std::uint32_t>(candidate.primitives.size()));
    for (std::uint32_t node_index = 0; node_index < source.nodes.size(); ++node_index) {
      const auto& node = source.nodes[node_index];
      if (node.mesh == gltf::invalid_index)
        continue;
      if (node.mesh >= source.meshes.size())
        return gpu_scene_plan_error::invalid_scene;
      const auto first = mesh_primitive_starts[node.mesh];
      const auto end = mesh_primitive_starts[node.mesh + 1];
      for (auto primitive_index = first; primitive_index < end; ++primitive_index) {
        if (candidate.draws.size() >= std::numeric_limits<std::uint32_t>::max())
          return gpu_scene_plan_error::numeric_overflow;
        packed_draw draw{.payload = static_cast<std::uint64_t>(candidate.draws.size()) + 1,
                         .primitive = primitive_index,
                         .material = candidate.primitives[primitive_index].material,
                         .node = node_index};
        append_draw_bounds(source.meshes[node.mesh].primitives[primitive_index - first], node,
                           draw);
        candidate.draws.push_back(draw);
        candidate.renderables.push_back(
            {.model = draw.model,
             .normal_matrix = draw.normal_matrix,
             .bounds_center = draw.bounds_center,
             .bounds_radius = draw.bounds_radius,
             .layer_mask = std::numeric_limits<std::uint64_t>::max(),
             .sort_key = (static_cast<std::uint64_t>(draw.material) << 32U) | draw.primitive,
             .payload = draw.payload,
             .object_id = static_cast<std::uint32_t>(candidate.draws.size()),
             .reserved = 0});
      }
    }
    for (const auto& material : source.materials) {
      append_texture(candidate.textures, material.base_color_texture, true);
      append_texture(candidate.textures, material.emissive_texture, true);
      append_texture(candidate.textures, material.metallic_roughness_texture, false);
      append_texture(candidate.textures, material.normal_texture, false);
      append_texture(candidate.textures, material.occlusion_texture, false);
    }
    candidate.source_sampler_to_plan.reserve(source.samplers.size());
    for (const auto& source_sampler : source.samplers) {
      const auto key = normalize_sampler(source_sampler);
      const auto found = std::ranges::find(candidate.samplers, key);
      if (found == candidate.samplers.end()) {
        if (candidate.samplers.size() >= std::numeric_limits<std::uint32_t>::max())
          return gpu_scene_plan_error::numeric_overflow;
        candidate.samplers.push_back(key);
        candidate.source_sampler_to_plan.push_back(
            static_cast<std::uint32_t>(candidate.samplers.size() - 1));
      } else {
        candidate.source_sampler_to_plan.push_back(static_cast<std::uint32_t>(
            static_cast<std::size_t>(found - candidate.samplers.begin())));
      }
    }
    for (const auto variant : candidate.textures) {
      if (variant.image >= source.images.size())
        return gpu_scene_plan_error::invalid_scene;
    }
    output = std::move(candidate);
    return gpu_scene_plan_error::none;
  } catch (const std::bad_alloc&) {
    return gpu_scene_plan_error::out_of_memory;
  } catch (const std::length_error&) {
    return gpu_scene_plan_error::numeric_overflow;
  }
}

gpu_scene::gpu_scene(gpu_scene&& other) noexcept
    : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)), plan_(std::move(other.plan_)),
      vertex_buffer_(std::move(other.vertex_buffer_)),
      index_buffer_(std::move(other.index_buffer_)), textures_(std::move(other.textures_)),
      samplers_(std::move(other.samplers_)), meshes_(std::move(other.meshes_)),
      default_textures_(std::move(other.default_textures_)),
      default_sampler_(std::move(other.default_sampler_)), materials_(std::move(other.materials_)),
      draw_bindings_(std::move(other.draw_bindings_)) {}

gpu_scene& gpu_scene::operator=(gpu_scene&& other) noexcept {
  if (this != &other) {
    reset();
    renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
    plan_ = std::move(other.plan_);
    vertex_buffer_ = std::move(other.vertex_buffer_);
    index_buffer_ = std::move(other.index_buffer_);
    textures_ = std::move(other.textures_);
    samplers_ = std::move(other.samplers_);
    meshes_ = std::move(other.meshes_);
    default_textures_ = std::move(other.default_textures_);
    default_sampler_ = std::move(other.default_sampler_);
    materials_ = std::move(other.materials_);
    draw_bindings_ = std::move(other.draw_bindings_);
  }
  return *this;
}

granit::result gpu_scene::initialize(granit_renderer renderer, const gltf::scene& source,
                                     float sampler_anisotropy) {
  gpu_scene candidate;
  const auto result = candidate.create(renderer, source, sampler_anisotropy);
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
                                          granit_texture_view& view,
                                          granit_sampler& sampler) const noexcept {
  view = GRANIT_NULL_HANDLE;
  sampler = GRANIT_NULL_HANDLE;
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
  view = texture->view.native_handle();
  sampler = selected_sampler->native_handle();
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
  granit_scene_snapshot_desc desc = GRANIT_SCENE_SNAPSHOT_DESC_INIT;
  desc.views = views.data();
  desc.view_count = static_cast<std::uint32_t>(views.size());
  desc.renderables = plan_.renderables.data();
  desc.renderable_count = static_cast<std::uint32_t>(plan_.renderables.size());
  desc.directional_lights = directional_lights.data();
  desc.directional_light_count = static_cast<std::uint32_t>(directional_lights.size());
  desc.point_lights = point_lights.data();
  desc.point_light_count = static_cast<std::uint32_t>(point_lights.size());
  desc.spot_lights = spot_lights.data();
  desc.spot_light_count = static_cast<std::uint32_t>(spot_lights.size());
  return output.initialize(renderer_, desc);
}

granit::result gpu_scene::update_material_factors(gltf::scene& source, std::uint32_t material_index,
                                                  const material_factor_edit& edit) noexcept {
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
      granit_material_parameter_update{granit::material_parameter_id("base_color"),
                                       GRANIT_MATERIAL_PARAMETER_FLOAT4, 0, &edit.base_color,
                                       sizeof(edit.base_color), 0},
      granit_material_parameter_update{granit::material_parameter_id("metallic"),
                                       GRANIT_MATERIAL_PARAMETER_FLOAT32, 0, &edit.metallic,
                                       sizeof(edit.metallic), 0},
      granit_material_parameter_update{granit::material_parameter_id("perceptual_roughness"),
                                       GRANIT_MATERIAL_PARAMETER_FLOAT32, 0, &edit.roughness,
                                       sizeof(edit.roughness), 0},
      granit_material_parameter_update{granit::material_parameter_id("normal_scale"),
                                       GRANIT_MATERIAL_PARAMETER_FLOAT32, 0, &edit.normal_scale,
                                       sizeof(edit.normal_scale), 0},
      granit_material_parameter_update{
          granit::material_parameter_id("occlusion_strength"), GRANIT_MATERIAL_PARAMETER_FLOAT32, 0,
          &edit.occlusion_strength, sizeof(edit.occlusion_strength), 0},
      granit_material_parameter_update{granit::material_parameter_id("emissive"),
                                       GRANIT_MATERIAL_PARAMETER_FLOAT3, 0, &edit.emissive,
                                       sizeof(edit.emissive), 0},
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
  const granit_material_parameter_update update{granit::material_parameter_id("debug_display"),
                                                GRANIT_MATERIAL_PARAMETER_UINT32,
                                                0,
                                                &mode,
                                                sizeof(mode),
                                                0};
  for (auto& material : materials_) {
    if (const auto result = material.update(std::span{&update, 1}); result.failed())
      return result;
  }
  return granit::result::success;
}

granit::result gpu_scene::create(granit_renderer renderer, const gltf::scene& source,
                                 float sampler_anisotropy) {
  if (renderer == GRANIT_NULL_HANDLE)
    return granit::result::invalid_handle;
  if (!std::isfinite(sampler_anisotropy) || sampler_anisotropy < 1.0F)
    return granit::result::invalid_argument;
  const auto plan_result = build_gpu_scene_plan(source, plan_);
  if (plan_result != gpu_scene_plan_error::none)
    return plan_result == gpu_scene_plan_error::out_of_memory ? granit::result::out_of_memory
                                                              : granit::result::invalid_argument;

  granit::upload_batch uploads;
  if (const auto result = uploads.initialize(renderer); result.failed())
    return result;
  if (!plan_.vertices.empty()) {
    const auto size = plan_.vertices.size() * sizeof(packed_vertex);
    if (const auto result = vertex_buffer_.initialize(
            renderer,
            {.size = size,
             .usage = granit::buffer_usage::vertex | granit::buffer_usage::transfer_destination,
             .location = granit::memory_location::device});
        result.failed())
      return result;
    if (const auto result = uploads.write_buffer(vertex_buffer_.native_handle(), 0,
                                                 std::as_bytes(std::span{plan_.vertices}));
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
    if (const auto result = uploads.write_buffer(index_buffer_.native_handle(), 0,
                                                 std::as_bytes(std::span{plan_.indices}));
        result.failed())
      return result;
  }

  textures_.reserve(plan_.textures.size());
  for (const auto variant : plan_.textures) {
    const auto& source_image = source.images[variant.image];
    if (source_image.mips.empty())
      return granit::result::invalid_argument;
    gpu_texture target;
    target.variant = variant;
    const auto& base_mip = source_image.mips.front();
    std::vector<std::byte> generated_pixels;
    std::vector<gltf::image_mip> generated_mips;
    const auto generate_mips = source_image.mips.size() == 1 &&
                               full_mip_count(base_mip.width, base_mip.height) > 1;
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
             .usage = granit::texture_usage::sampled |
                      granit::texture_usage::transfer_destination,
             .location = granit::memory_location::device,
             .width = base_mip.width,
             .height = base_mip.height,
             .mip_levels = mip_levels});
        result.failed())
      return result;
    if (const auto result =
            target.view.initialize(renderer, target.texture.native_handle(),
                                   {.format = variant.srgb ? granit::texture_format::rgba8_srgb
                                                           : granit::texture_format::rgba8_unorm,
                                    .mip_level_count = mip_levels});
        result.failed())
      return result;
    for (std::uint32_t mip_index = 0; mip_index < mips.size(); ++mip_index) {
      const auto& mip = mips[mip_index];
      if (mip.width > std::numeric_limits<std::uint32_t>::max() / 4 ||
          mip.offset > pixels.size() || mip.size > pixels.size() - mip.offset)
        return granit::result::invalid_argument;
      const auto bytes = std::span{pixels}.subspan(mip.offset, mip.size);
      const auto tight_row = mip.width * 4;
      const auto aligned_row =
          (std::uint64_t{tight_row} + texture_upload_row_alignment - 1) &
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
              target.texture.native_handle(), upload_bytes,
              {.bytes_per_row = row_pitch, .rows_per_image = mip.height},
              {.mip_level = mip_index, .width = mip.width, .height = mip.height});
          result.failed())
        return result;
    }
    textures_.push_back(std::move(target));
  }

  samplers_.reserve(plan_.samplers.size());
  for (const auto& key : plan_.samplers) {
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
  }

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
      granit_vertex_attribute{0, GRANIT_VERTEX_FORMAT_FLOAT32X3,
                              static_cast<std::uint32_t>(offsetof(packed_vertex, position)), 0},
      granit_vertex_attribute{1, GRANIT_VERTEX_FORMAT_FLOAT32X3,
                              static_cast<std::uint32_t>(offsetof(packed_vertex, normal)), 0},
      granit_vertex_attribute{2, GRANIT_VERTEX_FORMAT_FLOAT32X4,
                              static_cast<std::uint32_t>(offsetof(packed_vertex, tangent)), 0},
      granit_vertex_attribute{
          3, GRANIT_VERTEX_FORMAT_FLOAT32X2,
          static_cast<std::uint32_t>(offsetof(packed_vertex, texture_coordinate)), 0},
  };
  const granit_vertex_buffer_layout layout{sizeof(packed_vertex), GRANIT_VERTEX_STEP_MODE_VERTEX,
                                           static_cast<std::uint32_t>(attributes.size()), 0,
                                           attributes.data()};
  meshes_.reserve(plan_.primitives.size());
  for (const auto& primitive : plan_.primitives) {
    const granit_mesh_vertex_buffer binding{vertex_buffer_.native_handle(), primitive.vertex_offset,
                                            layout};
    granit_mesh_desc desc = GRANIT_MESH_DESC_INIT;
    desc.vertex_buffers = &binding;
    desc.vertex_buffer_count = 1;
    desc.indexed = 1;
    desc.index_buffer = index_buffer_.native_handle();
    desc.index_buffer_offset = primitive.index_offset;
    desc.index_type = GRANIT_INDEX_TYPE_UINT32;
    desc.vertex_count = primitive.vertex_count;
    desc.index_count = primitive.index_count;
    meshes_.emplace_back();
    if (const auto result = meshes_.back().initialize(renderer, desc); result.failed())
      return result;
  }
  if (const auto result = uploads.submit(); result.failed())
    return result;
  materials_.reserve(source.materials.size() + 1);
  for (const auto& source_material : source.materials) {
    materials_.emplace_back();
    if (const auto result = create_material(renderer, source_material, plan_, textures_, samplers_,
                                            default_textures_, default_sampler_, materials_.back());
        result.failed())
      return result;
  }
  materials_.emplace_back();
  if (const auto result = create_material(renderer, {}, plan_, textures_, samplers_,
                                          default_textures_, default_sampler_, materials_.back());
      result.failed())
    return result;

  draw_bindings_.reserve(plan_.draws.size());
  for (const auto& draw : plan_.draws) {
    const auto material_index =
        draw.material == gltf::invalid_index ? source.materials.size() : draw.material;
    draw_bindings_.push_back({.payload = draw.payload,
                              .mesh = meshes_[draw.primitive].native_handle(),
                              .material = materials_[material_index].native_handle(),
                              .reserved = 0});
  }
  renderer_ = renderer;
  return granit::result::success;
}

} // namespace granit::example::model_viewer
