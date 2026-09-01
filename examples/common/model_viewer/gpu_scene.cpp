// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/gpu_scene.h"

#include <granit/renderer/upload_batch.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace granit::example::model_viewer {
namespace {

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
  if (output.vertices.size() > std::numeric_limits<std::uint64_t>::max() / sizeof(packed_vertex) ||
      output.indices.size() > std::numeric_limits<std::uint64_t>::max() / sizeof(std::uint32_t))
    return gpu_scene_plan_error::numeric_overflow;

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
        if (candidate.draws.size() == std::numeric_limits<std::uint64_t>::max())
          return gpu_scene_plan_error::numeric_overflow;
        packed_draw draw{.payload = static_cast<std::uint64_t>(candidate.draws.size()) + 1,
                         .primitive = primitive_index,
                         .material = candidate.primitives[primitive_index].material,
                         .node = node_index};
        append_draw_bounds(source.meshes[node.mesh].primitives[primitive_index - first], node,
                           draw);
        candidate.draws.push_back(draw);
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
      samplers_(std::move(other.samplers_)), meshes_(std::move(other.meshes_)) {}

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
  }
  return *this;
}

granit::result gpu_scene::initialize(granit_renderer renderer, const gltf::scene& source) {
  gpu_scene candidate;
  const auto result = candidate.create(renderer, source);
  if (granit::failed(result))
    return result;
  *this = std::move(candidate);
  return granit::result::success;
}

void gpu_scene::reset() noexcept {
  if (!valid())
    return;
  [[maybe_unused]] gpu_scene retired(std::move(*this));
}

granit::result gpu_scene::create(granit_renderer renderer, const gltf::scene& source) {
  if (renderer == GRANIT_NULL_HANDLE)
    return granit::result::invalid_handle;
  const auto plan_result = build_gpu_scene_plan(source, plan_);
  if (plan_result != gpu_scene_plan_error::none)
    return plan_result == gpu_scene_plan_error::out_of_memory ? granit::result::out_of_memory
                                                              : granit::result::invalid_argument;

  granit::upload_batch uploads;
  const bool has_uploads =
      !plan_.vertices.empty() || !plan_.indices.empty() || !plan_.textures.empty();
  if (has_uploads) {
    if (const auto result = uploads.initialize(renderer); granit::failed(result))
      return result;
  }
  if (!plan_.vertices.empty()) {
    const auto size = plan_.vertices.size() * sizeof(packed_vertex);
    if (const auto result = vertex_buffer_.initialize(
            renderer,
            {.size = size,
             .usage = granit::buffer_usage::vertex | granit::buffer_usage::transfer_destination,
             .location = granit::memory_location::device});
        granit::failed(result))
      return result;
    if (const auto result = uploads.write_buffer(vertex_buffer_.native_handle(), 0,
                                                 std::as_bytes(std::span{plan_.vertices}));
        granit::failed(result))
      return result;
  }
  if (!plan_.indices.empty()) {
    const auto size = plan_.indices.size() * sizeof(std::uint32_t);
    if (const auto result =
            index_buffer_.initialize(renderer, {.size = size,
                                                .usage = granit::buffer_usage::index |
                                                         granit::buffer_usage::transfer_destination,
                                                .location = granit::memory_location::device});
        granit::failed(result))
      return result;
    if (const auto result = uploads.write_buffer(index_buffer_.native_handle(), 0,
                                                 std::as_bytes(std::span{plan_.indices}));
        granit::failed(result))
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
    if (const auto result = target.texture.initialize(
            renderer,
            {.format = variant.srgb ? granit::texture_format::rgba8_srgb
                                    : granit::texture_format::rgba8_unorm,
             .usage = granit::texture_usage::sampled | granit::texture_usage::transfer_destination,
             .location = granit::memory_location::device,
             .width = base_mip.width,
             .height = base_mip.height,
             .mip_levels = static_cast<std::uint32_t>(source_image.mips.size())});
        granit::failed(result))
      return result;
    if (const auto result = target.view.initialize(
            renderer, target.texture.native_handle(),
            {.format = variant.srgb ? granit::texture_format::rgba8_srgb
                                    : granit::texture_format::rgba8_unorm,
             .mip_level_count = static_cast<std::uint32_t>(source_image.mips.size())});
        granit::failed(result))
      return result;
    for (std::uint32_t mip_index = 0; mip_index < source_image.mips.size(); ++mip_index) {
      const auto& mip = source_image.mips[mip_index];
      if (mip.width > std::numeric_limits<std::uint32_t>::max() / 4 ||
          mip.offset > source_image.rgba8_pixels.size() ||
          mip.size > source_image.rgba8_pixels.size() - mip.offset)
        return granit::result::invalid_argument;
      const auto bytes = std::span{source_image.rgba8_pixels}.subspan(mip.offset, mip.size);
      if (const auto result = uploads.write_texture(
              target.texture.native_handle(), bytes,
              {.bytes_per_row = mip.width * 4, .rows_per_image = mip.height},
              {.mip_level = mip_index, .width = mip.width, .height = mip.height});
          granit::failed(result))
        return result;
    }
    textures_.push_back(std::move(target));
  }

  samplers_.reserve(plan_.samplers.size());
  for (const auto& key : plan_.samplers) {
    samplers_.emplace_back();
    if (const auto result =
            samplers_.back().initialize(renderer, {.mag_filter = key.mag_filter,
                                                   .min_filter = key.min_filter,
                                                   .mip_filter = key.mip_filter,
                                                   .address_u = key.address_u,
                                                   .address_v = key.address_v,
                                                   .address_w = granit::address_mode::repeat,
                                                   .max_lod = 1000.0F});
        granit::failed(result))
      return result;
  }

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
    if (const auto result = meshes_.back().initialize(renderer, desc); granit::failed(result))
      return result;
  }
  if (has_uploads) {
    if (const auto result = uploads.submit(); granit::failed(result))
      return result;
  }
  renderer_ = renderer;
  return granit::result::success;
}

} // namespace granit::example::model_viewer
