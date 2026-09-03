// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_GLTF_SCENE_H_
#define GRANIT_EXAMPLES_COMMON_GLTF_SCENE_H_

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <granit/math/types.hpp>

namespace granit::example::gltf {

inline constexpr std::uint32_t invalid_index = std::numeric_limits<std::uint32_t>::max();

struct bounds {
  math::float3 minimum{};
  math::float3 maximum{};
  bool valid{};
};

struct primitive {
  std::vector<std::uint32_t> indices;
  std::vector<math::float3> positions;
  std::vector<math::float3> normals;
  std::vector<math::float4> tangents;
  std::vector<math::float2> texture_coordinates;
  std::uint32_t material{invalid_index};
  bounds local_bounds{};
};

struct mesh {
  std::string name;
  std::vector<primitive> primitives;
};

struct texture_reference {
  std::uint32_t image{invalid_index};
  std::uint32_t sampler{invalid_index};
};

struct material {
  std::string name;
  math::float4 base_color{1.0F, 1.0F, 1.0F, 1.0F};
  float metallic{1.0F};
  float roughness{1.0F};
  float normal_scale{1.0F};
  float occlusion_strength{1.0F};
  math::float3 emissive{};
  texture_reference base_color_texture{};
  texture_reference metallic_roughness_texture{};
  texture_reference normal_texture{};
  texture_reference occlusion_texture{};
  texture_reference emissive_texture{};
};

struct image_mip {
  std::uint32_t width{};
  std::uint32_t height{};
  std::size_t offset{};
  std::size_t size{};
};

struct image {
  std::string name;
  std::vector<std::byte> rgba8_pixels;
  std::vector<image_mip> mips;
};

struct sampler {
  std::uint32_t mag_filter{};
  std::uint32_t min_filter{};
  std::uint32_t wrap_u{};
  std::uint32_t wrap_v{};
};

struct node {
  std::string name;
  std::uint32_t parent{invalid_index};
  std::vector<std::uint32_t> children;
  std::uint32_t mesh{invalid_index};
  math::matrix4 local_transform{math::identity_matrix4};
  math::matrix4 world_transform{math::identity_matrix4};
};

struct scene {
  std::vector<node> nodes;
  std::vector<std::uint32_t> roots;
  std::vector<mesh> meshes;
  std::vector<material> materials;
  std::vector<image> images;
  std::vector<sampler> samplers;
};

} // namespace granit::example::gltf

#endif
