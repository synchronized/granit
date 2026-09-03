// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/pbr_draw_inputs.h"

#include <algorithm>
#include <cmath>

namespace granit::material {
namespace {

bool finite(pbr_float3 value) noexcept {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool finite(const pbr_matrix4& value) noexcept {
  return std::ranges::all_of(value, [](float component) { return std::isfinite(component); });
}

} // namespace

pbr_draw_input_error pack_pbr_draw_inputs(const pbr_view_input& view,
                                          const pbr_object_input& object,
                                          const pbr_directional_light_input& light,
                                          pbr_frame_constants& frame_constants,
                                          pbr_object_constants& object_constants) noexcept {
  if (!finite(view.view_projection) || !finite(view.camera_position) || !finite(object.model) ||
      !finite(object.normal_matrix) || !finite(light.direction_to_light) ||
      !finite(light.radiance)) {
    return pbr_draw_input_error::non_finite_value;
  }
  const auto length_squared = light.direction_to_light.x * light.direction_to_light.x +
                              light.direction_to_light.y * light.direction_to_light.y +
                              light.direction_to_light.z * light.direction_to_light.z;
  if (length_squared <= 0.0F)
    return pbr_draw_input_error::invalid_light_direction;
  if (light.radiance.x < 0.0F || light.radiance.y < 0.0F || light.radiance.z < 0.0F)
    return pbr_draw_input_error::negative_light_radiance;

  const auto inverse_length = 1.0F / std::sqrt(length_squared);
  frame_constants = {.view_projection = view.view_projection,
                     .camera_position = {view.camera_position.x, view.camera_position.y,
                                         view.camera_position.z, 0.0F},
                     .direction_to_light = {light.direction_to_light.x * inverse_length,
                                            light.direction_to_light.y * inverse_length,
                                            light.direction_to_light.z * inverse_length, 0.0F},
                     .light_radiance = {light.radiance.x, light.radiance.y, light.radiance.z, 0.0F},
                     .render_options = {0, 0, 0, 0}};
  object_constants = {.model = object.model,
                      .normal_matrix = object.normal_matrix,
                      .object_id = {object.object_id, 0, 0, 0}};
  return pbr_draw_input_error::none;
}

} // namespace granit::material
