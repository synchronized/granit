// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "scene/scene_pbr_adapter.h"

#include <new>
#include <utility>

namespace granit::scene {

scene_pbr_error build_scene_pbr_pass_desc(const multi_view_snapshot& snapshot,
                                          std::size_t view_index, render_graph::resource_id color,
                                          render_graph::resource_id depth,
                                          scene_pbr_pass_desc& output) noexcept {
  if (view_index >= snapshot.views().size())
    return scene_pbr_error::invalid_view;
  if (color == render_graph::invalid_resource_id)
    return scene_pbr_error::invalid_attachment;
  const auto& view = snapshot.views()[view_index];
  const auto visible = view.renderables.indices();
  if (visible.empty())
    return scene_pbr_error::no_visible_renderables;
  if (view.directional_lights.size() != 1)
    return scene_pbr_error::directional_light_count;

  try {
    scene_pbr_pass_desc candidate;
    candidate.pbr.color = color;
    candidate.pbr.depth = depth;
    candidate.pbr.view = {.view_projection = view.view.view_projection,
                          .camera_position = {view.view.camera_position.x,
                                              view.view.camera_position.y,
                                              view.view.camera_position.z}};
    const auto& light = snapshot.directional_lights()[view.directional_lights.front()];
    candidate.pbr.light = {.direction_to_light = {light.direction_to_light.x,
                                                  light.direction_to_light.y,
                                                  light.direction_to_light.z},
                           .radiance = {light.radiance.x, light.radiance.y, light.radiance.z}};
    candidate.renderable_indices.assign(visible.begin(), visible.end());
    candidate.pbr.objects.reserve(visible.size());
    for (const auto index : visible) {
      const auto& renderable = snapshot.renderables()[index];
      candidate.pbr.objects.push_back({.model = renderable.model,
                                       .normal_matrix = renderable.normal_matrix,
                                       .object_id = renderable.object_id});
    }
    output = std::move(candidate);
    return scene_pbr_error::none;
  } catch (const std::bad_alloc&) {
    return scene_pbr_error::out_of_memory;
  } catch (...) {
    return scene_pbr_error::out_of_memory;
  }
}

render_graph::pass_id
add_scene_pbr_graph_pass(render_graph::serial_graph& graph, const multi_view_snapshot& snapshot,
                         std::size_t view_index, render_graph::resource_id color,
                         render_graph::resource_id depth, scene_pbr_record_callback callback,
                         scene_pbr_error& error, std::string name) {
  error = scene_pbr_error::none;
  if (!callback) {
    error = scene_pbr_error::invalid_callback;
    return render_graph::invalid_pass_id;
  }
  scene_pbr_pass_desc desc;
  error = build_scene_pbr_pass_desc(snapshot, view_index, color, depth, desc);
  if (error != scene_pbr_error::none)
    return render_graph::invalid_pass_id;
  auto indices = desc.renderable_indices;
  const auto pass = material::add_pbr_graph_pass(
      graph, std::move(desc.pbr),
      [callback = std::move(callback), indices = std::move(indices)](
          render_graph::pass_context& context, const material::pbr_frame_constants& frame,
          std::span<const material::pbr_object_constants> objects) {
        return callback(context, frame, objects, indices);
      },
      std::move(name));
  if (pass == render_graph::invalid_pass_id)
    error = scene_pbr_error::pass_rejected;
  return pass;
}

} // namespace granit::scene
