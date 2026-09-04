// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/render_view_submission.h"

#include <algorithm>
#include <new>

namespace granit::pipeline::detail {
namespace {

granit_matrix4 convert(const math::matrix4& value) {
  granit_matrix4 result{};
  std::ranges::copy(value, result.elements);
  return result;
}

granit_float3 convert(math::float3 value) { return {value.x, value.y, value.z}; }

} // namespace

granit_result build_render_view_submission(
    const scene::multi_view_snapshot& snapshot, std::uint32_t view_index,
    const std::unordered_map<std::uint64_t, granit_render_pipeline_draw_binding>& bindings,
    render_view_submission& output) noexcept {
  if (view_index >= snapshot.views().size())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto& visible = snapshot.views()[view_index];
  if (visible.renderables.indices().empty())
    return GRANIT_ERROR_NOT_READY;

  try {
    render_view_submission result;
    const auto count = visible.renderables.indices().size();
    result.payloads.reserve(count);
    result.draw_bindings.reserve(count);
    result.renderables.reserve(count);
    result.pbr_objects.reserve(count);
    for (const auto index : visible.renderables.indices()) {
      if (index >= snapshot.renderables().size())
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto& source = snapshot.renderables()[index];
      const auto binding = bindings.find(source.payload);
      if (binding == bindings.end())
        return GRANIT_ERROR_INVALID_ARGUMENT;
      result.payloads.push_back(source.payload);
      result.draw_bindings.push_back(binding->second);
      result.renderables.push_back({.model = convert(source.model),
                                    .normal_matrix = convert(source.normal_matrix),
                                    .bounds_center = convert(source.bounds.center),
                                    .bounds_radius = source.bounds.radius,
                                    .layer_mask = source.layer_mask,
                                    .sort_key = source.sort_key,
                                    .payload = source.payload,
                                    .object_id = source.object_id,
                                    .reserved = 0});
      result.pbr_objects.push_back({.model = source.model,
                                    .normal_matrix = source.normal_matrix,
                                    .object_id = source.object_id});
    }
    result.view = {.view = convert(visible.view.view),
                   .projection = convert(visible.view.projection),
                   .view_projection = convert(visible.view.view_projection),
                   .camera_position = convert(visible.view.camera_position),
                   .viewport_x = visible.view.area.x,
                   .viewport_y = visible.view.area.y,
                   .viewport_width = visible.view.area.width,
                   .viewport_height = visible.view.area.height,
                   .layer_mask = visible.view.layer_mask};
    output = std::move(result);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
}

} // namespace granit::pipeline::detail
