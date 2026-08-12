// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "scene/scene_visibility.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <tuple>
#include <utility>

namespace granit::scene {
namespace {

using row = std::array<float, 4>;

row matrix_row(const matrix4& matrix, std::size_t index) noexcept {
  return {matrix[index], matrix[index + 4], matrix[index + 8], matrix[index + 12]};
}

row add(row left, row right) noexcept {
  return {left[0] + right[0], left[1] + right[1], left[2] + right[2], left[3] + right[3]};
}

row subtract(row left, row right) noexcept {
  return {left[0] - right[0], left[1] - right[1], left[2] - right[2], left[3] - right[3]};
}

bool normalize(row value, frustum_plane& output) noexcept {
  const auto length_squared = value[0] * value[0] + value[1] * value[1] + value[2] * value[2];
  if (!std::isfinite(length_squared) || length_squared <= 0.0F)
    return false;
  const auto inverse_length = 1.0F / std::sqrt(length_squared);
  output = {{value[0] * inverse_length, value[1] * inverse_length, value[2] * inverse_length},
            value[3] * inverse_length};
  return std::isfinite(output.distance);
}

} // namespace

visibility_error extract_frustum(const matrix4& view_projection, frustum& output) noexcept {
  const auto row0 = matrix_row(view_projection, 0);
  const auto row1 = matrix_row(view_projection, 1);
  const auto row2 = matrix_row(view_projection, 2);
  const auto row3 = matrix_row(view_projection, 3);
  const std::array candidates{
      add(row3, row0),     subtract(row3, row0), add(row3, row1), subtract(row3, row1), row2,
      subtract(row3, row2)};
  frustum candidate{};
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    if (!normalize(candidates[index], candidate.planes[index]))
      return visibility_error::invalid_frustum;
  }
  output = candidate;
  return visibility_error::none;
}

bool intersects(const frustum& value, const bounding_sphere& bounds) noexcept {
  return std::ranges::all_of(value.planes, [&](const frustum_plane& plane) {
    const auto distance = plane.normal.x * bounds.center.x + plane.normal.y * bounds.center.y +
                          plane.normal.z * bounds.center.z + plane.distance;
    return distance >= -bounds.radius;
  });
}

visibility_error build_visible_list(const frame_snapshot& snapshot, visible_list& output) noexcept {
  const auto renderables = snapshot.renderables();
  if (renderables.size() > std::numeric_limits<std::uint32_t>::max())
    return visibility_error::too_many_renderables;
  frustum view_frustum{};
  const auto frustum_error = extract_frustum(snapshot.view().view_projection, view_frustum);
  if (frustum_error != visibility_error::none)
    return frustum_error;
  try {
    visible_list candidate;
    candidate.indices_.reserve(renderables.size());
    for (std::size_t index = 0; index < renderables.size(); ++index) {
      const auto& renderable = renderables[index];
      if ((snapshot.view().layer_mask & renderable.layer_mask) != 0 &&
          intersects(view_frustum, renderable.bounds)) {
        candidate.indices_.push_back(static_cast<std::uint32_t>(index));
      }
    }
    std::ranges::stable_sort(candidate.indices_, {}, [&](std::uint32_t index) {
      const auto& value = renderables[index];
      return std::tuple{value.sort_key, value.object_id};
    });
    output = std::move(candidate);
    return visibility_error::none;
  } catch (const std::bad_alloc&) {
    return visibility_error::out_of_memory;
  } catch (...) {
    return visibility_error::out_of_memory;
  }
}

} // namespace granit::scene
