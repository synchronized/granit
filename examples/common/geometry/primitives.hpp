// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_GEOMETRY_PRIMITIVES_HPP_
#define GRANIT_EXAMPLES_COMMON_GEOMETRY_PRIMITIVES_HPP_

#include <array>
#include <cstdint>

namespace granit::example::geometry {

struct normal_vertex {
  float position[3];
  float normal[3];
};

// 立方体使用独立面顶点，保证每个面的法线和索引绕序明确。
inline constexpr std::array<normal_vertex, 24> cube_vertices{{
    {{-1, -1, -1}, {0, 0, -1}}, {{1, -1, -1}, {0, 0, -1}}, {{1, 1, -1}, {0, 0, -1}},
    {{-1, 1, -1}, {0, 0, -1}},  {{-1, -1, 1}, {0, 0, 1}},  {{1, -1, 1}, {0, 0, 1}},
    {{1, 1, 1}, {0, 0, 1}},     {{-1, 1, 1}, {0, 0, 1}},   {{-1, -1, -1}, {-1, 0, 0}},
    {{-1, -1, 1}, {-1, 0, 0}},  {{-1, 1, 1}, {-1, 0, 0}},  {{-1, 1, -1}, {-1, 0, 0}},
    {{1, -1, -1}, {1, 0, 0}},   {{1, 1, -1}, {1, 0, 0}},   {{1, 1, 1}, {1, 0, 0}},
    {{1, -1, 1}, {1, 0, 0}},    {{-1, 1, -1}, {0, 1, 0}},  {{-1, 1, 1}, {0, 1, 0}},
    {{1, 1, 1}, {0, 1, 0}},     {{1, 1, -1}, {0, 1, 0}},   {{-1, -1, -1}, {0, -1, 0}},
    {{1, -1, -1}, {0, -1, 0}},  {{1, -1, 1}, {0, -1, 0}},  {{-1, -1, 1}, {0, -1, 0}},
}};

// 索引顺序遵循 Granit 逻辑裁剪空间的默认正面绕序。
inline constexpr std::array<std::uint16_t, 36> cube_indices{
    0,  1,  2,  0,  2,  3,  4,  7,  6,  4,  6,  5,  8,  11, 10, 8,  10, 9,
    12, 15, 14, 12, 14, 13, 16, 19, 18, 16, 18, 17, 20, 23, 22, 20, 22, 21,
};

// 09 使用的低面数 UV 球体，顶点法线与单位球面方向一致。
inline constexpr std::array<normal_vertex, 26> low_poly_sphere_vertices = [] {
  std::array<normal_vertex, 26> result{};
  constexpr std::array<std::array<float, 2>, 8> circle{{
      {{1.0F, 0.0F}},
      {{0.707107F, 0.707107F}},
      {{0.0F, 1.0F}},
      {{-0.707107F, 0.707107F}},
      {{-1.0F, 0.0F}},
      {{-0.707107F, -0.707107F}},
      {{0.0F, -1.0F}},
      {{0.707107F, -0.707107F}},
  }};
  result[0] = {{0.0F, 1.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
  for (std::uint32_t ring = 0; ring < 3; ++ring) {
    const float y = 0.707107F - static_cast<float>(ring) * 0.707107F;
    const float radius = ring == 1 ? 1.0F : 0.707107F;
    for (std::uint32_t segment = 0; segment < 8; ++segment) {
      const auto index = 1 + ring * 8 + segment;
      const auto& point = circle[segment];
      const normal_vertex value{{point[0] * radius, y, point[1] * radius},
                                {point[0] * radius, y, point[1] * radius}};
      result[index] = value;
    }
  }
  result[25] = {{0.0F, -1.0F, 0.0F}, {0.0F, -1.0F, 0.0F}};
  return result;
}();

inline constexpr std::array<std::uint16_t, 144> low_poly_sphere_indices = [] {
  std::array<std::uint16_t, 144> result{};
  std::uint32_t output = 0;
  for (std::uint32_t segment = 0; segment < 8; ++segment) {
    const auto next = (segment + 1) % 8;
    result[output++] = 0;
    result[output++] = static_cast<std::uint16_t>(1 + segment);
    result[output++] = static_cast<std::uint16_t>(1 + next);
  }
  for (std::uint32_t ring = 0; ring < 2; ++ring) {
    const auto first = 1 + ring * 8;
    const auto second = first + 8;
    for (std::uint32_t segment = 0; segment < 8; ++segment) {
      const auto next = (segment + 1) % 8;
      result[output++] = static_cast<std::uint16_t>(first + segment);
      result[output++] = static_cast<std::uint16_t>(second + segment);
      result[output++] = static_cast<std::uint16_t>(second + next);
      result[output++] = static_cast<std::uint16_t>(first + segment);
      result[output++] = static_cast<std::uint16_t>(second + next);
      result[output++] = static_cast<std::uint16_t>(first + next);
    }
  }
  for (std::uint32_t segment = 0; segment < 8; ++segment) {
    const auto next = (segment + 1) % 8;
    result[output++] = 25;
    result[output++] = static_cast<std::uint16_t>(17 + next);
    result[output++] = static_cast<std::uint16_t>(17 + segment);
  }
  return result;
}();

} // namespace granit::example::geometry

#endif
