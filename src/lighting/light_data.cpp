// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/light_data.h"

#include <cmath>
#include <limits>
#include <new>

namespace granit::lighting {
namespace {

bool to_u32(std::size_t value, std::uint32_t& output) noexcept {
  if (value > std::numeric_limits<std::uint32_t>::max())
    return false;
  output = static_cast<std::uint32_t>(value);
  return true;
}

gpu_directional_light pack(const scene::directional_light_input& light) noexcept {
  return {{light.direction_to_light.x, light.direction_to_light.y, light.direction_to_light.z},
          0.0F,
          {light.radiance.x, light.radiance.y, light.radiance.z},
          0.0F};
}

gpu_point_light pack(const scene::point_light_input& light) noexcept {
  return {{light.position.x, light.position.y, light.position.z},
          light.radius,
          {light.intensity.x, light.intensity.y, light.intensity.z},
          0.0F};
}

gpu_spot_light pack(const scene::spot_light_input& light) noexcept {
  return {{light.position.x, light.position.y, light.position.z},    light.radius,
          {light.direction.x, light.direction.y, light.direction.z}, std::cos(light.outer_angle),
          {light.intensity.x, light.intensity.y, light.intensity.z}, std::cos(light.inner_angle)};
}

} // namespace

light_pack_error pack_view_lights(const scene::multi_view_snapshot& snapshot,
                                  std::size_t view_index, const light_limits& limits,
                                  packed_view_lights& output,
                                  light_requirements& requirements) noexcept {
  if (view_index >= snapshot.views().size())
    return light_pack_error::view_out_of_range;

  const auto& view = snapshot.views()[view_index];
  if (!to_u32(view.directional_lights.size(), requirements.directional) ||
      !to_u32(view.point_lights.size(), requirements.point) ||
      !to_u32(view.spot_lights.size(), requirements.spot))
    return light_pack_error::capacity_exceeded;

  if (limits.directional > maximum_directional_lights || limits.point > maximum_point_lights ||
      limits.spot > maximum_spot_lights)
    return light_pack_error::invalid_limits;
  if (requirements.directional > limits.directional || requirements.point > limits.point ||
      requirements.spot > limits.spot)
    return light_pack_error::capacity_exceeded;

  try {
    packed_view_lights candidate;
    candidate.directional.reserve(requirements.directional);
    candidate.point.reserve(requirements.point);
    candidate.spot.reserve(requirements.spot);
    for (const auto index : view.directional_lights)
      candidate.directional.push_back(pack(snapshot.directional_lights()[index]));
    for (const auto index : view.point_lights)
      candidate.point.push_back(pack(snapshot.point_lights()[index]));
    for (const auto index : view.spot_lights)
      candidate.spot.push_back(pack(snapshot.spot_lights()[index]));
    output = std::move(candidate);
  } catch (const std::bad_alloc&) {
    return light_pack_error::out_of_memory;
  }
  return light_pack_error::none;
}

} // namespace granit::lighting
