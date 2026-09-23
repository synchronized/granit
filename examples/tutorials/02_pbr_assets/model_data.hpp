// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TUTORIAL_02_MODEL_DATA_HPP_
#define GRANIT_TUTORIAL_02_MODEL_DATA_HPP_

#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

namespace tutorial_model {

struct vertex {
  float position[3];
  float normal[3];
  float tangent[4];
  float uv[2];
};

struct mesh_data {
  std::vector<vertex> vertices;
  std::vector<std::uint16_t> indices;
};

/** 生成确定性的 UV Sphere，提供 PBR 所需的位置、法线、切线和 UV。 */
inline mesh_data make_uv_sphere(std::uint32_t rings = 24, std::uint32_t segments = 32) {
  mesh_data mesh;
  mesh.vertices.reserve((rings + 1U) * (segments + 1U));
  mesh.indices.reserve(rings * segments * 6U);

  constexpr float pi = std::numbers::pi_v<float>;
  for (std::uint32_t ring = 0; ring <= rings; ++ring) {
    const float v = static_cast<float>(ring) / static_cast<float>(rings);
    const float latitude = v * pi;
    const float y = std::cos(latitude);
    const float radius = std::sin(latitude);
    for (std::uint32_t segment = 0; segment <= segments; ++segment) {
      const float u = static_cast<float>(segment) / static_cast<float>(segments);
      const float longitude = u * pi * 2.0F;
      const float x = radius * std::cos(longitude);
      const float z = radius * std::sin(longitude);
      mesh.vertices.push_back(
          {{x, y, z}, {x, y, z}, {-std::sin(longitude), 0.0F, std::cos(longitude), 1.0F}, {u, v}});
    }
  }

  const auto row_size = segments + 1U;
  for (std::uint32_t ring = 0; ring < rings; ++ring) {
    for (std::uint32_t segment = 0; segment < segments; ++segment) {
      const auto first = static_cast<std::uint16_t>(ring * row_size + segment);
      const auto next = static_cast<std::uint16_t>(first + row_size);
      mesh.indices.insert(mesh.indices.end(), {first, next, static_cast<std::uint16_t>(first + 1U),
                                               static_cast<std::uint16_t>(first + 1U), next,
                                               static_cast<std::uint16_t>(next + 1U)});
    }
  }
  return mesh;
}

} // namespace tutorial_model

#endif
