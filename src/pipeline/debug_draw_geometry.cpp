// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/debug_draw_geometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace granit::pipeline::detail {
namespace {

using vector4 = std::array<float, 4>;

vector4 transform(const granit_matrix4& matrix, const granit_debug_draw_vertex& vertex) {
  const auto& m = matrix.elements;
  return {m[0] * vertex.x + m[4] * vertex.y + m[8] * vertex.z + m[12],
          m[1] * vertex.x + m[5] * vertex.y + m[9] * vertex.z + m[13],
          m[2] * vertex.x + m[6] * vertex.y + m[10] * vertex.z + m[14],
          m[3] * vertex.x + m[7] * vertex.y + m[11] * vertex.z + m[15]};
}

vector4 interpolate(const vector4& first, const vector4& second, float amount) {
  vector4 result{};
  for (std::size_t index = 0; index < result.size(); ++index)
    result[index] = first[index] + (second[index] - first[index]) * amount;
  return result;
}

std::uint32_t interpolate_color(std::uint32_t first, std::uint32_t second, float amount) {
  std::uint32_t result = 0;
  for (std::uint32_t shift = 0; shift < 32; shift += 8) {
    const auto a = static_cast<float>((first >> shift) & UINT32_C(0xff));
    const auto b = static_cast<float>((second >> shift) & UINT32_C(0xff));
    const auto value = static_cast<std::uint32_t>(std::lround(a + (b - a) * amount));
    result |= std::min(value, UINT32_C(0xff)) << shift;
  }
  return result;
}

bool clip_interval(const vector4& first, const vector4& second, float& begin, float& end) {
  constexpr float minimum_w = 1.0e-6F;
  const std::array first_planes{
      first[0] + first[3], first[3] - first[0], first[1] + first[3], first[3] - first[1], first[2],
      first[3] - first[2], first[3] - minimum_w};
  const std::array second_planes{second[0] + second[3],
                                 second[3] - second[0],
                                 second[1] + second[3],
                                 second[3] - second[1],
                                 second[2],
                                 second[3] - second[2],
                                 second[3] - minimum_w};
  for (std::size_t index = 0; index < first_planes.size(); ++index) {
    const auto a = first_planes[index];
    const auto b = second_planes[index];
    if (a < 0 && b < 0)
      return false;
    if ((a < 0) != (b < 0)) {
      const auto crossing = a / (a - b);
      if (a < 0)
        begin = std::max(begin, crossing);
      else
        end = std::min(end, crossing);
      if (begin > end)
        return false;
    }
  }
  return true;
}

bool finite(const vector4& value) {
  return std::ranges::all_of(value, [](float component) { return std::isfinite(component); });
}

} // namespace

debug_line_expand_result
expand_world_debug_line(const granit_debug_draw_line& line, const granit_matrix4& view_projection,
                        std::uint32_t viewport_width, std::uint32_t viewport_height,
                        std::array<debug_clip_vertex, 4>& output) noexcept {
  if (line.space != GRANIT_DEBUG_DRAW_SPACE_WORLD || !std::isfinite(line.width) ||
      line.width <= 0 || viewport_width == 0 || viewport_height == 0)
    return debug_line_expand_result::invalid_argument;
  const auto original_start = transform(view_projection, line.start);
  const auto original_end = transform(view_projection, line.end);
  if (!finite(original_start) || !finite(original_end))
    return debug_line_expand_result::invalid_argument;
  float begin = 0;
  float end = 1;
  if (!clip_interval(original_start, original_end, begin, end))
    return debug_line_expand_result::clipped;
  const auto start = interpolate(original_start, original_end, begin);
  const auto finish = interpolate(original_start, original_end, end);
  const auto start_x = start[0] / start[3];
  const auto start_y = start[1] / start[3];
  const auto end_x = finish[0] / finish[3];
  const auto end_y = finish[1] / finish[3];
  const auto dx = (end_x - start_x) * static_cast<float>(viewport_width) * 0.5F;
  const auto dy = (end_y - start_y) * static_cast<float>(viewport_height) * -0.5F;
  const auto length = std::sqrt(dx * dx + dy * dy);
  if (!std::isfinite(length) || length <= 1.0e-6F)
    return debug_line_expand_result::clipped;
  const auto half_width = line.width * 0.5F;
  const auto pixel_x = -dy / length * half_width;
  const auto pixel_y = dx / length * half_width;
  const auto ndc_x = pixel_x * 2.0F / static_cast<float>(viewport_width);
  const auto ndc_y = pixel_y * -2.0F / static_cast<float>(viewport_height);
  const auto start_color = interpolate_color(line.start.color, line.end.color, begin);
  const auto end_color = interpolate_color(line.start.color, line.end.color, end);
  output = {{{{start[0] + ndc_x * start[3], start[1] + ndc_y * start[3], start[2], start[3]},
              start_color},
             {{start[0] - ndc_x * start[3], start[1] - ndc_y * start[3], start[2], start[3]},
              start_color},
             {{finish[0] + ndc_x * finish[3], finish[1] + ndc_y * finish[3], finish[2], finish[3]},
              end_color},
             {{finish[0] - ndc_x * finish[3], finish[1] - ndc_y * finish[3], finish[2], finish[3]},
              end_color}}};
  return debug_line_expand_result::success;
}

} // namespace granit::pipeline::detail
