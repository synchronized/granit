// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "gltf_rendering/scene_resources.h"

namespace granit::example::gltf_rendering {

granit::result
scene_resources::create_snapshot(std::span<const granit_scene_view> views,
                                 std::span<const granit_scene_directional_light> directional_lights,
                                 std::span<const granit_scene_point_light> point_lights,
                                 std::span<const granit_scene_spot_light> spot_lights,
                                 granit::scene_snapshot& output) const noexcept {
  if (!valid())
    return granit::result::invalid_handle;
  if (views.size() > std::numeric_limits<std::uint32_t>::max() ||
      plan_.renderables.size() > std::numeric_limits<std::uint32_t>::max() ||
      directional_lights.size() > std::numeric_limits<std::uint32_t>::max() ||
      point_lights.size() > std::numeric_limits<std::uint32_t>::max() ||
      spot_lights.size() > std::numeric_limits<std::uint32_t>::max())
    return granit::result::invalid_argument;
  const granit::scene_snapshot_desc desc{
      .views = views,
      .renderables = plan_.renderables,
      .directional_lights = directional_lights,
      .point_lights = point_lights,
      .spot_lights = spot_lights,
  };
  return output.initialize(renderer_, desc);
}

} // namespace granit::example::gltf_rendering
