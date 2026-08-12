// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/directional_shadow.h"

#include <algorithm>
#include <cmath>
#include <new>
#include <utility>

namespace granit::lighting {
namespace {

bool valid_volume(const directional_shadow_volume& volume) noexcept {
  return math::is_finite(volume.focus) && std::isfinite(volume.half_width) &&
         std::isfinite(volume.half_height) && std::isfinite(volume.near_plane) &&
         std::isfinite(volume.far_plane) && std::isfinite(volume.light_distance) &&
         volume.half_width > 0.0F && volume.half_height > 0.0F && volume.near_plane >= 0.0F &&
         volume.far_plane > volume.near_plane && volume.light_distance > 0.0F;
}

bool contains(std::span<const std::uint32_t> values, std::uint32_t target) noexcept {
  return std::ranges::find(values, target) != values.end();
}

} // namespace

directional_shadow_error build_directional_shadow_pass_desc(
    const scene::multi_view_snapshot& snapshot, std::size_t view_index,
    std::uint32_t directional_light_index, const directional_shadow_volume& volume,
    render_graph::resource_id depth, directional_shadow_pass_desc& output) noexcept {
  if (view_index >= snapshot.views().size())
    return directional_shadow_error::view_out_of_range;
  if (directional_light_index >= snapshot.directional_lights().size())
    return directional_shadow_error::light_out_of_range;
  const auto& target_view = snapshot.views()[view_index];
  if (!contains(target_view.directional_lights, directional_light_index))
    return directional_shadow_error::light_not_visible;
  if (!valid_volume(volume))
    return directional_shadow_error::invalid_volume;
  if (depth == render_graph::invalid_resource_id)
    return directional_shadow_error::invalid_depth;

  const auto& light = snapshot.directional_lights()[directional_light_index];
  const auto direction = math::normalize(light.direction_to_light);
  const auto eye = math::add(volume.focus, math::multiply(direction, volume.light_distance));
  const auto up = std::abs(math::dot(direction, {0.0F, 1.0F, 0.0F})) < 0.99F
                      ? math::float3{0.0F, 1.0F, 0.0F}
                      : math::float3{1.0F, 0.0F, 0.0F};
  math::matrix4 view{};
  math::matrix4 projection{};
  if (!math::look_at_rh(eye, volume.focus, up, view) ||
      !math::orthographic_rh_zo(-volume.half_width, volume.half_width, -volume.half_height,
                                volume.half_height, volume.near_plane, volume.far_plane,
                                projection)) {
    return directional_shadow_error::invalid_volume;
  }
  const auto view_projection = math::multiply(projection, view);
  scene::view_input shadow_view{.view = view,
                                .projection = projection,
                                .view_projection = view_projection,
                                .camera_position = eye,
                                .area = {},
                                .layer_mask = target_view.view.layer_mask & light.layer_mask};
  scene::visible_list visible;
  const auto visibility = scene::build_visible_list(shadow_view, snapshot.renderables(), visible);
  if (visibility == scene::visibility_error::invalid_frustum)
    return directional_shadow_error::invalid_frustum;
  if (visibility != scene::visibility_error::none)
    return directional_shadow_error::out_of_memory;
  if (visible.indices().empty())
    return directional_shadow_error::no_casters;

  try {
    directional_shadow_pass_desc candidate;
    candidate.depth = depth;
    candidate.frame.light_view_projection = view_projection;
    candidate.casters.reserve(visible.indices().size());
    for (const auto index : visible.indices()) {
      const auto& renderable = snapshot.renderables()[index];
      candidate.casters.push_back({.model = renderable.model,
                                   .payload = renderable.payload,
                                   .object_id = renderable.object_id,
                                   .source_index = index});
    }
    output = std::move(candidate);
  } catch (const std::bad_alloc&) {
    return directional_shadow_error::out_of_memory;
  }
  return directional_shadow_error::none;
}

render_graph::pass_id add_directional_shadow_graph_pass(render_graph::serial_graph& graph,
                                                        directional_shadow_pass_desc desc,
                                                        directional_shadow_record_callback callback,
                                                        std::string name) {
  if (desc.depth == render_graph::invalid_resource_id || desc.casters.empty() || !callback)
    return render_graph::invalid_pass_id;
  render_graph::pass_desc pass{.side_effect = true,
                               .accesses = {{desc.depth, render_graph::access_type::write}}};
  return graph.add_pass(
      std::move(pass),
      [frame = desc.frame, casters = std::move(desc.casters), callback = std::move(callback)](
          render_graph::pass_context& context) { return callback(context, frame, casters); },
      std::move(name));
}

} // namespace granit::lighting
