// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/gpu_scene.h"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>

namespace granit::example::model_viewer {
namespace {

void append_texture(std::vector<texture_variant>& output, const gltf::texture_reference& reference,
                    bool srgb) {
  if (reference.image == gltf::invalid_index)
    return;
  const texture_variant variant{.image = reference.image, .srgb = srgb};
  if (std::ranges::find(output, variant) == output.end())
    output.push_back(variant);
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
    for (const auto& mesh : source.meshes) {
      for (const auto& primitive : mesh.primitives) {
        if (const auto result = append_primitive(primitive, candidate);
            result != gpu_scene_plan_error::none)
          return result;
      }
    }
    for (const auto& material : source.materials) {
      append_texture(candidate.textures, material.base_color_texture, true);
      append_texture(candidate.textures, material.emissive_texture, true);
      append_texture(candidate.textures, material.metallic_roughness_texture, false);
      append_texture(candidate.textures, material.normal_texture, false);
      append_texture(candidate.textures, material.occlusion_texture, false);
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

} // namespace granit::example::model_viewer
