// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/scene.h>

#include "pipeline/scene_access.h"
#include "scene/multi_view_submission.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>

namespace {

constexpr uint64_t index_mask = UINT64_C(0xffffffff);
constexpr uint64_t generation_mask = UINT64_C(0x00ffffff);
constexpr uint64_t type_value = UINT64_C(0x40);

struct snapshot_state {
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  granit::scene::multi_view_snapshot snapshot;
};

struct slot {
  std::unique_ptr<snapshot_state> state;
  uint32_t generation = 1;
};

std::mutex registry_mutex;
std::vector<slot> registry;

granit_handle encode(size_t index, uint32_t generation) {
  return (type_value << 56) | (static_cast<uint64_t>(generation) << 32) |
         (static_cast<uint64_t>(index) + 1);
}

bool decode(granit_handle handle, size_t& index, uint32_t& generation) {
  if ((handle >> 56) != type_value || (handle & index_mask) == 0)
    return false;
  index = static_cast<size_t>((handle & index_mask) - 1);
  generation = static_cast<uint32_t>((handle >> 32) & generation_mask);
  return generation != 0;
}

granit::math::float3 convert(granit_float3 value) { return {value.x, value.y, value.z}; }

granit::math::matrix4 convert(const granit_matrix4& value) {
  granit::math::matrix4 result{};
  std::copy_n(value.elements, result.size(), result.begin());
  return result;
}

template <typename T> bool valid_array(const T* values, uint32_t count) {
  return count == 0 || values != nullptr;
}

granit_result validate_renderer(granit_renderer renderer) {
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  uint64_t size = 0;
  return granit_renderer_pipeline_cache_export(renderer, nullptr, &size);
}

granit_result map_error(granit::scene::multi_view_error error) {
  if (error == granit::scene::multi_view_error::out_of_memory)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  return GRANIT_ERROR_INVALID_ARGUMENT;
}

} // namespace

granit_result
granit::pipeline::detail::copy_scene_snapshot(granit_renderer renderer,
                                              granit_scene_snapshot snapshot,
                                              scene::multi_view_snapshot& output) noexcept {
  size_t index = 0;
  uint32_t generation = 0;
  if (!decode(snapshot, index, generation))
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    std::scoped_lock lock{registry_mutex};
    if (index >= registry.size() || registry[index].state == nullptr ||
        registry[index].generation != generation || registry[index].state->renderer != renderer) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    output = registry[index].state->snapshot;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_scene_snapshot_create(granit_renderer renderer,
                                                      const granit_scene_snapshot_desc* desc,
                                                      granit_scene_snapshot* snapshot) {
  if (snapshot == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *snapshot = GRANIT_NULL_HANDLE;
  if (desc == nullptr || desc->struct_size < GRANIT_SCENE_SNAPSHOT_DESC_VERSION_1_SIZE ||
      desc->reserved != 0 || !valid_array(desc->views, desc->view_count) ||
      !valid_array(desc->renderables, desc->renderable_count) ||
      !valid_array(desc->directional_lights, desc->directional_light_count) ||
      !valid_array(desc->point_lights, desc->point_light_count) ||
      !valid_array(desc->spot_lights, desc->spot_light_count)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto renderer_result = validate_renderer(renderer);
  if (renderer_result != GRANIT_SUCCESS)
    return renderer_result;

  try {
    std::vector<granit::scene::view_input> views;
    std::vector<granit::scene::renderable_input> renderables;
    std::vector<granit::scene::directional_light_input> directional;
    std::vector<granit::scene::point_light_input> points;
    std::vector<granit::scene::spot_light_input> spots;
    views.reserve(desc->view_count);
    renderables.reserve(desc->renderable_count);
    directional.reserve(desc->directional_light_count);
    points.reserve(desc->point_light_count);
    spots.reserve(desc->spot_light_count);
    for (uint32_t index = 0; index < desc->view_count; ++index) {
      const auto& source = desc->views[index];
      views.push_back({.view = convert(source.view),
                       .projection = convert(source.projection),
                       .view_projection = convert(source.view_projection),
                       .camera_position = convert(source.camera_position),
                       .area = {source.viewport_x, source.viewport_y, source.viewport_width,
                                source.viewport_height},
                       .layer_mask = source.layer_mask});
    }
    for (uint32_t index = 0; index < desc->renderable_count; ++index) {
      const auto& source = desc->renderables[index];
      renderables.push_back({.model = convert(source.model),
                             .normal_matrix = convert(source.normal_matrix),
                             .bounds = {convert(source.bounds_center), source.bounds_radius},
                             .layer_mask = source.layer_mask,
                             .sort_key = source.sort_key,
                             .payload = source.payload,
                             .object_id = source.object_id});
    }
    for (uint32_t index = 0; index < desc->directional_light_count; ++index) {
      const auto& source = desc->directional_lights[index];
      directional.push_back(
          {convert(source.direction_to_light), convert(source.radiance), source.layer_mask});
    }
    for (uint32_t index = 0; index < desc->point_light_count; ++index) {
      const auto& source = desc->point_lights[index];
      points.push_back(
          {convert(source.position), convert(source.intensity), source.radius, source.layer_mask});
    }
    for (uint32_t index = 0; index < desc->spot_light_count; ++index) {
      const auto& source = desc->spot_lights[index];
      spots.push_back({convert(source.position), convert(source.direction),
                       convert(source.intensity), source.radius, source.inner_angle,
                       source.outer_angle, source.layer_mask});
    }

    auto state = std::make_unique<snapshot_state>();
    state->renderer = renderer;
    const auto error = granit::scene::build_multi_view_snapshot({.views = views,
                                                                 .renderables = renderables,
                                                                 .directional_lights = directional,
                                                                 .point_lights = points,
                                                                 .spot_lights = spots},
                                                                state->snapshot);
    if (error != granit::scene::multi_view_error::none)
      return map_error(error);

    std::scoped_lock lock{registry_mutex};
    size_t index = 0;
    while (index < registry.size() && registry[index].state != nullptr)
      ++index;
    if (index == registry.size())
      registry.emplace_back();
    registry[index].state = std::move(state);
    *snapshot = encode(index, registry[index].generation);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_scene_snapshot_destroy(granit_renderer renderer,
                                                       granit_scene_snapshot snapshot) {
  size_t index = 0;
  uint32_t generation = 0;
  if (!decode(snapshot, index, generation))
    return GRANIT_ERROR_INVALID_HANDLE;
  std::unique_ptr<snapshot_state> removed;
  {
    std::scoped_lock lock{registry_mutex};
    if (index >= registry.size() || registry[index].state == nullptr ||
        registry[index].generation != generation || registry[index].state->renderer != renderer) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    removed = std::move(registry[index].state);
    registry[index].generation =
        registry[index].generation == generation_mask ? 1 : registry[index].generation + 1;
  }
  return GRANIT_SUCCESS;
}
