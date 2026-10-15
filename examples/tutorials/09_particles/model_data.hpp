// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_TUTORIALS_09_PARTICLES_MODEL_DATA_HPP_
#define GRANIT_EXAMPLES_TUTORIALS_09_PARTICLES_MODEL_DATA_HPP_

#include <array>
#include <cstdint>

namespace tutorial_particles {

struct vertex {
  float position[3];
  float normal[3];
};

// 前 26 个顶点组成八段低面数 UV 球，后 8 个顶点组成带法线的立方体。
inline constexpr std::array<vertex, 34> vertices = [] {
  std::array<vertex, 34> result{};
  constexpr std::array<std::array<float, 2>, 8> circle{{
      {{1.0F, 0.0F}}, {{0.707107F, 0.707107F}}, {{0.0F, 1.0F}},
      {{-0.707107F, 0.707107F}}, {{-1.0F, 0.0F}}, {{-0.707107F, -0.707107F}},
      {{0.0F, -1.0F}}, {{0.707107F, -0.707107F}},
  }};
  result[0] = {{0.0F, 1.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
  for (std::uint32_t ring = 0; ring < 3; ++ring) {
    const float y = 0.707107F - static_cast<float>(ring) * 0.707107F;
    const float radius = ring == 1 ? 1.0F : 0.707107F;
    for (std::uint32_t segment = 0; segment < 8; ++segment) {
      const auto index = 1 + ring * 8 + segment;
      const auto& point = circle[segment];
      result[index] = {{point[0] * radius, y, point[1] * radius},
                       {point[0] * radius, y, point[1] * radius}};
    }
  }
  result[25] = {{0.0F, -1.0F, 0.0F}, {0.0F, -1.0F, 0.0F}};
  constexpr float normal = 0.57735F;
  constexpr std::array<std::array<float, 3>, 8> cube{{
      {{-1, -1, -1}}, {{1, -1, -1}}, {{1, 1, -1}}, {{-1, 1, -1}},
      {{-1, -1, 1}}, {{1, -1, 1}}, {{1, 1, 1}}, {{-1, 1, 1}},
  }};
  for (std::uint32_t index = 0; index < cube.size(); ++index)
    result[26 + index] = {{cube[index][0], cube[index][1], cube[index][2]},
                          {cube[index][0] * normal, cube[index][1] * normal,
                           cube[index][2] * normal}};
  return result;
}();

inline constexpr std::array<std::uint16_t, 180> indices = [] {
  std::array<std::uint16_t, 180> result{};
  std::uint32_t output = 0;
  for (std::uint32_t segment = 0; segment < 8; ++segment) {
    const auto next = (segment + 1) % 8;
    result[output++] = 0;
    result[output++] = static_cast<std::uint16_t>(1 + next);
    result[output++] = static_cast<std::uint16_t>(1 + segment);
  }
  for (std::uint32_t ring = 0; ring < 2; ++ring) {
    const auto first = 1 + ring * 8;
    const auto second = first + 8;
    for (std::uint32_t segment = 0; segment < 8; ++segment) {
      const auto next = (segment + 1) % 8;
      result[output++] = static_cast<std::uint16_t>(first + segment);
      result[output++] = static_cast<std::uint16_t>(second + next);
      result[output++] = static_cast<std::uint16_t>(second + segment);
      result[output++] = static_cast<std::uint16_t>(first + segment);
      result[output++] = static_cast<std::uint16_t>(first + next);
      result[output++] = static_cast<std::uint16_t>(second + next);
    }
  }
  for (std::uint32_t segment = 0; segment < 8; ++segment) {
    const auto next = (segment + 1) % 8;
    result[output++] = 25;
    result[output++] = static_cast<std::uint16_t>(17 + segment);
    result[output++] = static_cast<std::uint16_t>(17 + next);
  }
  constexpr std::array<std::uint16_t, 36> cube_indices{
      0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 4, 7, 0, 7, 3,
      1, 2, 6, 1, 6, 5, 3, 7, 6, 3, 6, 2, 0, 1, 5, 0, 5, 4,
  };
  for (const auto index : cube_indices)
    result[output++] = index;
  return result;
}();

} // namespace tutorial_particles

#endif
