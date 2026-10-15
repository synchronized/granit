// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_TUTORIALS_09_PARTICLES_MODEL_DATA_HPP_
#define GRANIT_EXAMPLES_TUTORIALS_09_PARTICLES_MODEL_DATA_HPP_

#include "geometry/primitives.hpp"

#include <array>
#include <cstdint>

namespace tutorial_particles {

using vertex = granit::example::geometry::normal_vertex;

// 粒子教程复用公共低面数球体和立方体；长方体通过实例缩放得到。
inline constexpr auto vertices = [] {
  std::array<vertex, 50> result{};
  for (std::uint32_t index = 0; index < granit::example::geometry::low_poly_sphere_vertices.size();
       ++index)
    result[index] = granit::example::geometry::low_poly_sphere_vertices[index];
  for (std::uint32_t index = 0; index < granit::example::geometry::cube_vertices.size(); ++index)
    result[26 + index] = granit::example::geometry::cube_vertices[index];
  return result;
}();

inline constexpr auto indices = [] {
  constexpr auto sphere_indices = granit::example::geometry::low_poly_sphere_indices;
  constexpr auto cube_indices = granit::example::geometry::cube_indices;
  std::array<std::uint16_t, sphere_indices.size() + cube_indices.size()> result{};
  for (std::uint32_t index = 0; index < sphere_indices.size(); ++index)
    result[index] = sphere_indices[index];
  for (std::uint32_t index = 0; index < cube_indices.size(); ++index)
    result[sphere_indices.size() + index] = cube_indices[index];
  return result;
}();

} // namespace tutorial_particles

#endif
